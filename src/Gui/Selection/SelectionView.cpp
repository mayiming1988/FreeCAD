/***************************************************************************
 *   Copyright (c) 2002 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>
#endif

#include <App/ComplexGeoData.h>
#include <App/Document.h>
#include <App/GeoFeature.h>

#include "SelectionView.h"
#include "Application.h"
#include "BitmapFactory.h"
#include "Command.h"
#include "Document.h"


FC_LOG_LEVEL_INIT("Selection", true, true, true)

using namespace Gui;
using namespace Gui::DockWnd;


/* TRANSLATOR Gui::DockWnd::SelectionView */

SelectionView::SelectionView(Gui::Document* pcDocument, QWidget* parent)
    : DockWindow(pcDocument, parent)
    , SelectionObserver(true, ResolveMode::NoResolve)
    , x(0.0f)
    , y(0.0f)
    , z(0.0f)
    , openedAutomatically(false)
{
    setWindowTitle(tr("Selection View"));

    QVBoxLayout* vLayout = new QVBoxLayout(this);
    vLayout->setSpacing(0);
    vLayout->setContentsMargins(0, 0, 0, 0);

    QLineEdit* searchBox = new QLineEdit(this);
    searchBox->setPlaceholderText(tr("Search"));
    searchBox->setToolTip(tr("Searches object labels"));
    QHBoxLayout* hLayout = new QHBoxLayout();
    hLayout->setSpacing(2);
    QToolButton* clearButton = new QToolButton(this);
    clearButton->setFixedSize(18, 21);
    clearButton->setCursor(Qt::ArrowCursor);
    clearButton->setStyleSheet(QStringLiteral("QToolButton {margin-bottom:1px}"));
    clearButton->setIcon(BitmapFactory().pixmap(":/icons/edit-cleartext.svg"));
    clearButton->setToolTip(tr("Clears the search field"));
    clearButton->setAutoRaise(true);
    countLabel = new QLabel(this);
    countLabel->setText(QStringLiteral("0"));
    countLabel->setToolTip(tr("The number of selected items"));
    hLayout->addWidget(searchBox);
    hLayout->addWidget(clearButton, 0, Qt::AlignRight);
    hLayout->addWidget(countLabel, 0, Qt::AlignRight);
    vLayout->addLayout(hLayout);

    selectionView = new QListWidget(this);
    selectionView->setContextMenuPolicy(Qt::CustomContextMenu);
    vLayout->addWidget(selectionView);

    enablePickList = new QCheckBox(this);
    enablePickList->setText(tr("Picked object list"));
    vLayout->addWidget(enablePickList);
    pickList = new QListWidget(this);
    pickList->setVisible(false);
    vLayout->addWidget(pickList);

    selectionView->setMouseTracking(true);  // needed for itemEntered() to work
    pickList->setMouseTracking(true);

    resize(200, 200);

    // clang-format off
    connect(clearButton, &QToolButton::clicked, searchBox, &QLineEdit::clear);
    connect(searchBox, &QLineEdit::textChanged, this, &SelectionView::search);
    connect(searchBox, &QLineEdit::editingFinished, this, &SelectionView::validateSearch);
    connect(selectionView, &QListWidget::itemDoubleClicked, this, &SelectionView::toggleSelect);
    connect(selectionView, &QListWidget::itemEntered, this, &SelectionView::preselect);
    connect(pickList, &QListWidget::itemDoubleClicked, this, &SelectionView::toggleSelect);
    connect(pickList, &QListWidget::itemEntered, this, &SelectionView::preselect);
    connect(selectionView, &QListWidget::customContextMenuRequested, this, &SelectionView::onItemContextMenu);
#if QT_VERSION >= QT_VERSION_CHECK(6,7,0)
    connect(enablePickList, &QCheckBox::checkStateChanged, this, &SelectionView::onEnablePickList);
#else
    connect(enablePickList, &QCheckBox::stateChanged, this, &SelectionView::onEnablePickList);
#endif
    // clang-format on
}

SelectionView::~SelectionView() = default;

void SelectionView::leaveEvent(QEvent*)
{
    Selection().rmvPreselect();
}

/// @cond DOXERR
void SelectionView::onSelectionChanged(const SelectionChanges& Reason)
{
    ParameterGrp::handle hGrp = App::GetApplication()
                                    .GetUserParameter()
                                    .GetGroup("BaseApp")
                                    ->GetGroup("Preferences")
                                    ->GetGroup("Selection");
    bool autoShow = hGrp->GetBool("AutoShowSelectionView", false);
    hGrp->SetBool("AutoShowSelectionView",
                  autoShow);  // Remove this line once the preferences window item is implemented

    if (autoShow) {
        if (!parentWidget()->isVisible() && Selection().hasSelection()) {
            parentWidget()->show();
            openedAutomatically = true;
        }
        else if (openedAutomatically && !Selection().hasSelection()) {
            parentWidget()->hide();
            openedAutomatically = false;
        }
    }

    QString selObject;
    QTextStream str(&selObject);

    auto getSelectionName = [](QTextStream& str,
                               const char* docName,
                               const char* objName,
                               const char* subName,
                               App::DocumentObject* obj) {
        str << docName;
        str << "#";
        str << objName;
        if (subName != 0 && subName[0] != 0) {
            str << ".";
            /* Original code doesn't take account of histories in subelement names and displays
             * them inadvertently.  Let's not do that.
            str << subName;
            */
            /* Remove the history from the displayed subelement name */
            App::ElementNamePair elementName;
            App::GeoFeature::resolveElement(obj, subName, elementName);
            str << elementName.oldName.c_str();  // Use the shortened element name not the full one.
            /* Mark it visually if there was a history as a "tell" for if a given selection has TNP
             * fixes in it. */
            if (elementName.newName.size() > 0) {
                str << " []";
            }
            auto subObj = obj->getSubObject(subName);
            if (subObj) {
                obj = subObj;
            }
        }
        str << " (";
        str << QString::fromUtf8(obj->Label.getValue());
        str << ")";
    };

    if (Reason.Type == SelectionChanges::AddSelection) {
        // save as user data
        QStringList list;
        list << QString::fromLatin1(Reason.pDocName);
        list << QString::fromLatin1(Reason.pObjectName);
        App::Document* doc = App::GetApplication().getDocument(Reason.pDocName);
        App::DocumentObject* obj = doc->getObject(Reason.pObjectName);
        getSelectionName(str, Reason.pDocName, Reason.pObjectName, Reason.pSubName, obj);

        // insert the selection as item
        QListWidgetItem* item = new QListWidgetItem(selObject, selectionView);
        item->setData(Qt::UserRole, list);
    }
    else if (Reason.Type == SelectionChanges::ClrSelection) {
        if (!Reason.pDocName[0]) {
            // remove all items
            selectionView->clear();
        }
        else {
            // build name
            str << Reason.pDocName;
            str << "#";
            // remove all items
            const auto items = selectionView->findItems(selObject, Qt::MatchStartsWith);
            for (auto item : items) {
                delete item;
            }
        }
    }
    else if (Reason.Type == SelectionChanges::RmvSelection) {
        App::Document* doc = App::GetApplication().getDocument(Reason.pDocName);
        App::DocumentObject* obj = doc->getObject(Reason.pObjectName);
        getSelectionName(str, Reason.pDocName, Reason.pObjectName, Reason.pSubName, obj);
        // remove all items
        QList<QListWidgetItem*> l = selectionView->findItems(selObject, Qt::MatchStartsWith);
        if (l.size() == 1) {
            delete l[0];
        }
    }
    else if (Reason.Type == SelectionChanges::SetSelection) {
        // remove all items
        selectionView->clear();
        std::vector<SelectionSingleton::SelObj> objs =
            Gui::Selection().getSelection(Reason.pDocName, ResolveMode::NoResolve);
        for (const auto& it : objs) {
            // save as user data
            QStringList list;
            list << QString::fromLatin1(it.DocName);
            list << QString::fromLatin1(it.FeatName);

            App::Document* doc = App::GetApplication().getDocument(it.DocName);
            App::DocumentObject* obj = doc->getObject(it.FeatName);
            getSelectionName(str, it.DocName, it.FeatName, it.SubName, obj);
            QListWidgetItem* item = new QListWidgetItem(selObject, selectionView);
            item->setData(Qt::UserRole, list);
            selObject.clear();
        }
    }
    else if (Reason.Type == SelectionChanges::PickedListChanged) {
        bool picking = Selection().needPickedList();
        enablePickList->setChecked(picking);
        pickList->setVisible(picking);
        pickList->clear();
        if (picking) {
            const auto& sels = Selection().getPickedList(Reason.pDocName);
            for (const auto& sel : sels) {
                App::Document* doc = App::GetApplication().getDocument(sel.DocName);
                if (!doc) {
                    continue;
                }
                App::DocumentObject* obj = doc->getObject(sel.FeatName);
                if (!obj) {
                    continue;
                }

                QString selObject;
                QTextStream str(&selObject);
                getSelectionName(str, sel.DocName, sel.FeatName, sel.SubName, obj);

                this->x = sel.x;
                this->y = sel.y;
                this->z = sel.z;

                new QListWidgetItem(selObject, pickList);
            }
        }
    }

    countLabel->setText(QString::number(selectionView->count()));
}

void SelectionView::search(const QString& text)
{
    if (!text.isEmpty()) {
        searchList.clear();
        App::Document* doc = App::GetApplication().getActiveDocument();
        std::vector<App::DocumentObject*> objects;
        if (doc) {
            objects = doc->getObjects();
            selectionView->clear();
            for (auto it : objects) {
                QString label = QString::fromUtf8(it->Label.getValue());
                if (label.contains(text, Qt::CaseInsensitive)) {
                    searchList.push_back(it);
                    // save as user data
                    QString selObject;
                    QTextStream str(&selObject);
                    QStringList list;
                    list << QString::fromLatin1(doc->getName());
                    list << QString::fromLatin1(it->getNameInDocument());
                    // build name
                    str << QString::fromUtf8(doc->Label.getValue());
                    str << "#";
                    str << it->getNameInDocument();
                    str << " (";
                    str << label;
                    str << ")";
                    QListWidgetItem* item = new QListWidgetItem(selObject, selectionView);
                    item->setData(Qt::UserRole, list);
                }
            }
            countLabel->setText(QString::number(selectionView->count()));
        }
    }
}

void SelectionView::validateSearch()
{
    if (!searchList.empty()) {
        App::Document* doc = App::GetApplication().getActiveDocument();
        if (doc) {
            Gui::Selection().clearSelection();
            for (auto it : searchList) {
                Gui::Selection().addSelection(doc->getName(), it->getNameInDocument(), nullptr);
            }
        }
    }
}

void SelectionView::select(QListWidgetItem* item)
{
    if (!item) {
        item = selectionView->currentItem();
    }
    if (!item) {
        return;
    }
    QStringList elements = item->data(Qt::UserRole).toStringList();
    if (elements.size() < 2) {
        return;
    }

    try {
        // Gui::Selection().clearSelection();
        Gui::Command::runCommand(Gui::Command::Gui, "Gui.Selection.clearSelection()");
        // Gui::Selection().addSelection(elements[0].toLatin1(),elements[1].toLatin1(),0);
        QString cmd = QStringLiteral(
                          R"(Gui.Selection.addSelection(App.getDocument("%1").getObject("%2")))")
                          .arg(elements[0], elements[1]);
        Gui::Command::runCommand(Gui::Command::Gui, cmd.toLatin1());
    }
    catch (Base::Exception& e) {
        e.reportException();
    }
}

void SelectionView::deselect()
{
    QListWidgetItem* item = selectionView->currentItem();
    if (!item) {
        return;
    }
    QStringList elements = item->data(Qt::UserRole).toStringList();
    if (elements.size() < 2) {
        return;
    }

    // Gui::Selection().rmvSelection(elements[0].toLatin1(),elements[1].toLatin1(),0);
    QString cmd = QStringLiteral(
                      R"(Gui.Selection.removeSelection(App.getDocument("%1").getObject("%2")))")
                      .arg(elements[0], elements[1]);
    try {
        Gui::Command::runCommand(Gui::Command::Gui, cmd.toLatin1());
    }
    catch (Base::Exception& e) {
        e.reportException();
    }
}

void SelectionView::toggleSelect(QListWidgetItem* item)
{
    if (!item) {
        return;
    }
    std::string name = item->text().toLatin1().constData();
    char* docname = &name.at(0);
    char* objname = std::strchr(docname, '#');
    if (!objname) {
        return;
    }
    *objname++ = 0;
    char* subname = std::strchr(objname, '.');
    if (subname) {
        *subname++ = 0;
        char* end = std::strchr(subname, ' ');
        if (end) {
            *end = 0;
        }
    }
    else {
        char* end = std::strchr(objname, ' ');
        if (end) {
            *end = 0;
        }
    }
    QString cmd;
    if (Gui::Selection().isSelected(docname, objname, subname)) {
        cmd = QStringLiteral("Gui.Selection.removeSelection("
                                  "App.getDocument('%1').getObject('%2'),'%3')")
                  .arg(QString::fromLatin1(docname),
                       QString::fromLatin1(objname),
                       QString::fromLatin1(subname));
    }
    else {
        cmd = QStringLiteral("Gui.Selection.addSelection("
                                  "App.getDocument('%1').getObject('%2'),'%3',%4,%5,%6)")
                  .arg(QString::fromLatin1(docname),
                       QString::fromLatin1(objname),
                       QString::fromLatin1(subname))
                  .arg(x)
                  .arg(y)
                  .arg(z);
    }
    try {
        Gui::Command::runCommand(Gui::Command::Gui, cmd.toLatin1());
    }
    catch (Base::Exception& e) {
        e.reportException();
    }
}

void SelectionView::preselect(QListWidgetItem* item)
{
    if (!item) {
        return;
    }
    std::string name = item->text().toLatin1().constData();
    char* docname = &name.at(0);
    char* objname = std::strchr(docname, '#');
    if (!objname) {
        return;
    }
    *objname++ = 0;
    char* subname = std::strchr(objname, '.');
    if (subname) {
        *subname++ = 0;
        char* end = std::strchr(subname, ' ');
        if (end) {
            *end = 0;
        }
    }
    else {
        char* end = std::strchr(objname, ' ');
        if (end) {
            *end = 0;
        }
    }
    QString cmd = QStringLiteral("Gui.Selection.setPreselection("
                                      "App.getDocument('%1').getObject('%2'),'%3',tp=2)")
                      .arg(QString::fromLatin1(docname),
                           QString::fromLatin1(objname),
                           QString::fromLatin1(subname));
    try {
        Gui::Command::runCommand(Gui::Command::Gui, cmd.toLatin1());
    }
    catch (Base::Exception& e) {
        e.reportException();
    }
}

void SelectionView::zoom()
{
    select();
    try {
        Gui::Command::runCommand(Gui::Command::Gui, "Gui.SendMsgToActiveView(\"ViewSelection\")");
    }
    catch (Base::Exception& e) {
        e.reportException();
    }
}

void SelectionView::treeSelect()
{
    select();
    try {
        Gui::Command::runCommand(Gui::Command::Gui, "Gui.runCommand(\"Std_TreeSelection\")");
    }
    catch (Base::Exception& e) {
        e.reportException();
    }
}

void SelectionView::touch()
{
    QListWidgetItem* item = selectionView->currentItem();
    if (!item) {
        return;
    }
    QStringList elements = item->data(Qt::UserRole).toStringList();
    if (elements.size() < 2) {
        return;
    }
    QString cmd = QStringLiteral(R"(App.getDocument("%1").getObject("%2").touch())")
                      .arg(elements[0], elements[1]);
    try {
        Gui::Command::runCommand(Gui::Command::Doc, cmd.toLatin1());
    }
    catch (Base::Exception& e) {
        e.reportException();
    }
}

void SelectionView::toPython()
{
    QListWidgetItem* item = selectionView->currentItem();
    if (!item) {
        return;
    }
    QStringList elements = item->data(Qt::UserRole).toStringList();
    if (elements.size() < 2) {
        return;
    }

    try {
        QString cmd = QStringLiteral(R"(obj = App.getDocument("%1").getObject("%2"))")
                          .arg(elements[0], elements[1]);
        Gui::Command::runCommand(Gui::Command::Gui, cmd.toLatin1());
        if (elements.length() > 2) {
            App::Document* doc = App::GetApplication().getDocument(elements[0].toLatin1());
            App::DocumentObject* obj = doc->getObject(elements[1].toLatin1());
            QString property = getProperty(obj);

            cmd = QStringLiteral(R"(shp = App.getDocument("%1").getObject("%2").%3)")
                      .arg(elements[0], elements[1], property);
            Gui::Command::runCommand(Gui::Command::Gui, cmd.toLatin1());

            if (supportPart(obj, elements[2])) {
                cmd = QStringLiteral(R"(elt = App.getDocument("%1").getObject("%2").%3.%4)")
                          .arg(elements[0], elements[1], property, elements[2]);
                Gui::Command::runCommand(Gui::Command::Gui, cmd.toLatin1());
            }
        }
    }
    catch (const Base::Exception& e) {
        e.reportException();
    }
}

void SelectionView::showPart()
{
    QListWidgetItem* item = selectionView->currentItem();
    if (!item) {
        return;
    }
    QStringList elements = item->data(Qt::UserRole).toStringList();
    if (elements.length() > 2) {
        App::Document* doc = App::GetApplication().getDocument(elements[0].toLatin1());
        App::DocumentObject* obj = doc->getObject(elements[1].toLatin1());
        QString module = getModule(obj->getTypeId().getName());
        QString property = getProperty(obj);
        if (!module.isEmpty() && !property.isEmpty() && supportPart(obj, elements[2])) {
            try {
                Gui::Command::addModule(Gui::Command::Gui, module.toLatin1());
                QString cmd =
                    QStringLiteral(R"(%1.show(App.getDocument("%2").getObject("%3").%4.%5))")
                        .arg(module, elements[0], elements[1], property, elements[2]);
                Gui::Command::runCommand(Gui::Command::Gui, cmd.toLatin1());
            }
            catch (const Base::Exception& e) {
                e.reportException();
            }
        }
    }
}

QString SelectionView::getModule(const char* type) const
{
    // go up the inheritance tree and find the module name of the first
    // sub-class that has not the prefix "App::"
    std::string prefix;
    Base::Type typeId = Base::Type::fromName(type);

    while (!typeId.isBad()) {
        std::string temp(typeId.getName());
        std::string::size_type pos = temp.find_first_of("::");

        std::string module;
        if (pos != std::string::npos) {
            module = std::string(temp, 0, pos);
        }
        if (module != "App") {
            prefix = module;
        }
        else {
            break;
        }
        typeId = typeId.getParent();
    }

    return QString::fromStdString(prefix);
}

QString SelectionView::getProperty(App::DocumentObject* obj) const
{
    QString property;
    if (obj->isDerivedFrom<App::GeoFeature>()) {
        App::GeoFeature* geo = static_cast<App::GeoFeature*>(obj);
        const App::PropertyComplexGeoData* data = geo->getPropertyOfGeometry();
        const char* name = data ? data->getName() : nullptr;
        if (App::Property::isValidName(name)) {
            property = QString::fromLatin1(name);
        }
    }

    return property;
}

bool SelectionView::supportPart(App::DocumentObject* obj, const QString& part) const
{
    if (obj->isDerivedFrom<App::GeoFeature>()) {
        App::GeoFeature* geo = static_cast<App::GeoFeature*>(obj);
        const App::PropertyComplexGeoData* data = geo->getPropertyOfGeometry();
        if (data) {
            const Data::ComplexGeoData* geometry = data->getComplexData();
            std::vector<const char*> types = geometry->getElementTypes();
            for (auto it : types) {
                if (part.startsWith(QString::fromLatin1(it))) {
                    return true;
                }
            }
        }
    }

    return false;
}

void SelectionView::onItemContextMenu(const QPoint& point)
{
    QListWidgetItem* item = selectionView->itemAt(point);
    if (!item) {
        return;
    }
    QMenu menu;
    QAction* selectAction = menu.addAction(tr("Select only"), this, [&] {
        this->select(nullptr);
    });
    selectAction->setIcon(QIcon::fromTheme(QStringLiteral("view-select")));
    selectAction->setToolTip(tr("Selects only this object"));

    QAction* deselectAction = menu.addAction(tr("Deselect"), this, &SelectionView::deselect);
    deselectAction->setIcon(QIcon::fromTheme(QStringLiteral("view-unselectable")));
    deselectAction->setToolTip(tr("Deselects this object"));

    QAction* zoomAction = menu.addAction(tr("Zoom fit"), this, &SelectionView::zoom);
    zoomAction->setIcon(QIcon::fromTheme(QStringLiteral("zoom-fit-best")));
    zoomAction->setToolTip(tr("Selects and fits this object in the 3D window"));

    QAction* gotoAction = menu.addAction(tr("Go to selection"), this, &SelectionView::treeSelect);
    gotoAction->setToolTip(tr("Selects and locates this object in the tree view"));

    QAction* touchAction = menu.addAction(tr("Mark to recompute"), this, &SelectionView::touch);
    touchAction->setIcon(QIcon::fromTheme(QStringLiteral("view-refresh")));
    touchAction->setToolTip(tr("Mark this object to be recomputed"));

    QAction* toPythonAction =
        menu.addAction(tr("To Python console"), this, &SelectionView::toPython);
    toPythonAction->setIcon(QIcon::fromTheme(QStringLiteral("applications-python")));
    toPythonAction->setToolTip(
        tr("Reveals this object and its subelements in the Python console."));

    QStringList elements = item->data(Qt::UserRole).toStringList();
    if (elements.length() > 2) {
        // subshape-specific entries
        QAction* showPart =
            menu.addAction(tr("Duplicate subshape"), this, &SelectionView::showPart);
        showPart->setIcon(QIcon(QStringLiteral(":/icons/ClassBrowser/member.svg")));
        showPart->setToolTip(tr("Creates a standalone copy of this subshape in the document"));
    }
    menu.exec(selectionView->mapToGlobal(point));
}

void SelectionView::onUpdate()
{}

bool SelectionView::onMsg(const char* /*pMsg*/, const char** /*ppReturn*/)
{
    return false;
}

void SelectionView::hideEvent(QHideEvent* ev)
{
    DockWindow::hideEvent(ev);
}

void SelectionView::showEvent(QShowEvent* ev)
{
    enablePickList->setChecked(Selection().needPickedList());
    Gui::DockWindow::showEvent(ev);
}

void SelectionView::onEnablePickList()
{
    bool enabled = enablePickList->isChecked();
    Selection().enablePickedList(enabled);
    pickList->setVisible(enabled);
}

/// @endcond

#include "moc_SelectionView.cpp"
