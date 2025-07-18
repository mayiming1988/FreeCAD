/***************************************************************************
 *   Copyright (c) 2015 Stefan Tröger <stefantroeger@gmx.net>              *
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
#include <Inventor/SoPickedPoint.h>
#include <Inventor/events/SoMouseButtonEvent.h>
#include <Inventor/nodes/SoCoordinate3.h>
#include <Inventor/nodes/SoMarkerSet.h>

#include <sstream>
#include <limits>
#include <QApplication>
#include <QMessageBox>
#include <QMetaMethod>
#include <QToolTip>
#endif

#include <App/Document.h>
#include <Base/Console.h>
#include <Base/UnitsApi.h>
#include <Gui/Action.h>
#include <Gui/Application.h>
#include <Gui/BitmapFactory.h>
#include <Gui/CommandT.h>
#include <Gui/Document.h>
#include <Gui/Inventor/MarkerBitmaps.h>
#include <Gui/MainWindow.h>
#include <Gui/View3DInventor.h>
#include <Gui/View3DInventorViewer.h>
#include <Mod/Fem/App/FemPostFilter.h>
#include <Mod/Fem/App/FemPostBranchFilter.h>
#include <Mod/Fem/App/FemPostPipeline.h>

#include "ui_TaskPostCalculator.h"
#include "ui_TaskPostClip.h"
#include "ui_TaskPostContours.h"
#include "ui_TaskPostCut.h"
#include "ui_TaskPostDataAlongLine.h"
#include "ui_TaskPostDataAtPoint.h"
#include "ui_TaskPostDisplay.h"
#include "ui_TaskPostScalarClip.h"
#include "ui_TaskPostWarpVector.h"
#include "ui_TaskPostFrames.h"
#include "ui_TaskPostBranch.h"

#include "FemSettings.h"
#include "TaskPostBoxes.h"
#include "ViewProviderFemPostFilter.h"
#include "ViewProviderFemPostFunction.h"
#include "ViewProviderFemPostObject.h"
#include "ViewProviderFemPostBranchFilter.h"

using namespace FemGui;
using namespace Gui;

// ***************************************************************************
// point marker
PointMarker::PointMarker(Gui::View3DInventorViewer* iv, App::DocumentObject* obj)
    : connSelectPoint(QMetaObject::Connection())
    , view(iv)
    , obj(obj)
    , vp(new ViewProviderPointMarker)
{
    view->addViewProvider(vp);
}

PointMarker::~PointMarker()
{
    view->removeViewProvider(vp);
    delete vp;
}

void PointMarker::addPoint(const SbVec3f& pt)
{
    int ct = countPoints();
    vp->pCoords->point.set1Value(ct, pt);
    vp->pMarker->numPoints = ct + 1;
}

void PointMarker::clearPoints() const
{
    vp->pMarker->numPoints = 0;
    vp->pCoords->point.setNum(0);
}

int PointMarker::countPoints() const
{
    return vp->pCoords->point.getNum();
}

SbVec3f PointMarker::getPoint(int idx) const
{
    return vp->pCoords->point[idx];
}

void PointMarker::setPoint(int idx, const SbVec3f& pt) const
{
    vp->pCoords->point.set1Value(idx, pt);
}

Gui::View3DInventorViewer* PointMarker::getView() const
{
    return view;
}

App::DocumentObject* PointMarker::getObject() const
{
    return obj;
}

std::string PointMarker::ObjectInvisible()
{
    return "for amesh in App.activeDocument().Objects:\n\
    if \"Mesh\" in amesh.TypeId:\n\
         aparttoshow = amesh.Name.replace(\"_Mesh\",\"\")\n\
         for apart in App.activeDocument().Objects:\n\
             if aparttoshow == apart.Name:\n\
                 apart.ViewObject.Visibility = False\n";
}


PROPERTY_SOURCE(FemGui::ViewProviderPointMarker, Gui::ViewProvider)

ViewProviderPointMarker::ViewProviderPointMarker()
{
    pCoords = new SoCoordinate3();
    pCoords->ref();
    pCoords->point.setNum(0);
    pMarker = new SoMarkerSet();
    pMarker->markerIndex = Gui::Inventor::MarkerBitmaps::getMarkerIndex(
        "CIRCLE_FILLED",
        App::GetApplication()
            .GetParameterGroupByPath("User parameter:BaseApp/Preferences/View")
            ->GetInt("MarkerSize", 9));
    pMarker->numPoints = 0;
    pMarker->ref();

    SoGroup* grp = new SoGroup();
    grp->addChild(pCoords);
    grp->addChild(pMarker);
    addDisplayMaskMode(grp, "Base");
    setDisplayMaskMode("Base");
}

ViewProviderPointMarker::~ViewProviderPointMarker()
{
    pCoords->unref();
    pMarker->unref();
}


// ***************************************************************************
// DataAlongLine marker
DataAlongLineMarker::DataAlongLineMarker(Gui::View3DInventorViewer* iv,
                                         Fem::FemPostDataAlongLineFilter* obj)
    : PointMarker(iv, obj)
{}

void DataAlongLineMarker::customEvent(QEvent*)
{
    const SbVec3f& pt1 = getPoint(0);
    const SbVec3f& pt2 = getPoint(1);

    Q_EMIT PointsChanged(pt1[0], pt1[1], pt1[2], pt2[0], pt2[1], pt2[2]);
    Gui::Command::doCommand(Gui::Command::Doc,
                            "App.ActiveDocument.%s.Point1 = App.Vector(%f, %f, %f)",
                            getObject()->getNameInDocument(),
                            pt1[0],
                            pt1[1],
                            pt1[2]);
    Gui::Command::doCommand(Gui::Command::Doc,
                            "App.ActiveDocument.%s.Point2 = App.Vector(%f, %f, %f)",
                            getObject()->getNameInDocument(),
                            pt2[0],
                            pt2[1],
                            pt2[2]);
    Gui::Command::doCommand(Gui::Command::Doc, ObjectInvisible().c_str());
}


// ***************************************************************************
// main task dialog
TaskPostWidget::TaskPostWidget(Gui::ViewProviderDocumentObject* view,
                               const QPixmap& icon,
                               const QString& title,
                               QWidget* parent)
    : QWidget(parent)
    , m_object(view->getObject())
    , m_view(view)
{
    setWindowTitle(title);
    setWindowIcon(icon);
    m_icon = icon;

    m_connection =
        m_object->signalChanged.connect(boost::bind(&TaskPostWidget::handlePropertyChange,
                                                    this,
                                                    boost::placeholders::_1,
                                                    boost::placeholders::_2));
}

TaskPostWidget::~TaskPostWidget()
{
    m_connection.disconnect();
};

bool TaskPostWidget::autoApply()
{
    return FemSettings().getPostAutoRecompute();
}

App::Document* TaskPostWidget::getDocument() const
{
    App::DocumentObject* obj = getObject();
    return (obj ? obj->getDocument() : nullptr);
}

void TaskPostWidget::recompute()
{
    if (autoApply()) {
        App::Document* doc = getDocument();
        if (doc) {
            doc->recompute();
        }
    }
}

void TaskPostWidget::updateEnumerationList(App::PropertyEnumeration& prop, QComboBox* box)
{
    QStringList list;
    std::vector<std::string> vec = prop.getEnumVector();
    for (auto it : vec) {
        list.push_back(QString::fromStdString(it));
    }

    int index = prop.getValue();
    // be aware the QComboxBox might be connected to the Property,
    // thus clearing the box will set back the property enumeration index too.
    // https://forum.freecad.org/viewtopic.php?f=10&t=30944
    box->clear();
    box->insertItems(0, list);
    box->setCurrentIndex(index);
}

void TaskPostWidget::handlePropertyChange(const App::DocumentObject& obj, const App::Property& prop)
{
    if (auto postobj = m_object.get<Fem::FemPostObject>()) {
        if (&prop == &postobj->Data) {
            this->onPostDataChanged(postobj);
        }
    }
}

// ***************************************************************************
// simulation dialog for the TaskView
TaskDlgPost::TaskDlgPost(Gui::ViewProviderDocumentObject* view)
    : TaskDialog()
    , m_view(view)
{
    assert(view);
}

TaskDlgPost::~TaskDlgPost() = default;

QDialogButtonBox::StandardButtons TaskDlgPost::getStandardButtons() const
{
    bool guionly = true;
    for (auto& widget : Content) {
        if (auto task_box = dynamic_cast<Gui::TaskView::TaskBox*>(widget)) {

            // get the task widget and check if it is a post widget
            auto widget = task_box->groupLayout()->itemAt(0)->widget();
            if (auto post_widget = dynamic_cast<TaskPostWidget*>(widget)) {
                guionly = guionly && post_widget->isGuiTaskOnly();
            }
            else {
                // unknown panel, we can only assume
                guionly = false;
            }
        }
    }

    if (!guionly) {
        return QDialogButtonBox::Apply | QDialogButtonBox::Ok | QDialogButtonBox::Cancel;
    }
    else {
        return QDialogButtonBox::Ok;
    }
}

void TaskDlgPost::connectSlots()
{
    // Connect emitAddedFunction() with slotAddedFunction()
    QObject* sender = nullptr;
    int indexSignal = 0;
    for (const auto dlg : Content) {
        indexSignal = dlg->metaObject()->indexOfSignal(
            QMetaObject::normalizedSignature("emitAddedFunction()"));
        if (indexSignal >= 0) {
            sender = dlg;
            break;
        }
    }

    if (sender) {
        for (const auto dlg : Content) {
            int indexSlot = dlg->metaObject()->indexOfSlot(
                QMetaObject::normalizedSignature("slotAddedFunction()"));
            if (indexSlot >= 0) {
                connect(sender,
                        sender->metaObject()->method(indexSignal),
                        dlg,
                        dlg->metaObject()->method(indexSlot));
            }
        }
    }
}

void TaskDlgPost::open()
{
    // only open a new command if none is pending (e.g. if the object was newly created)
    if (!Gui::Command::hasPendingCommand()) {
        auto text = std::string("Edit ") + m_view->getObject()->Label.getValue();
        Gui::Command::openCommand(text.c_str());
    }
}

void TaskDlgPost::clicked(int button)
{
    if (button == QDialogButtonBox::Apply) {
        for (auto& widget : Content) {
            if (auto task_box = dynamic_cast<Gui::TaskView::TaskBox*>(widget)) {
                // get the task widget and check if it is a post widget
                auto widget = task_box->groupLayout()->itemAt(0)->widget();
                if (auto post_widget = dynamic_cast<TaskPostWidget*>(widget)) {
                    post_widget->apply();
                }
            }
        }
        recompute();
    }
}

bool TaskDlgPost::accept()
{
    try {
        for (auto& widget : Content) {
            if (auto task_box = dynamic_cast<Gui::TaskView::TaskBox*>(widget)) {
                // get the task widget and check if it is a post widget
                auto widget = task_box->groupLayout()->itemAt(0)->widget();
                if (auto post_widget = dynamic_cast<TaskPostWidget*>(widget)) {
                    post_widget->applyPythonCode();
                }
            }
        }
    }
    catch (const Base::Exception& e) {
        QMessageBox::warning(nullptr, tr("Input error"), QString::fromLatin1(e.what()));
        return false;
    }

    Gui::cmdGuiDocument(getDocumentName(), "resetEdit()");
    return true;
}

bool TaskDlgPost::reject()
{
    // roll back the done things
    Gui::Command::abortCommand();
    Gui::cmdGuiDocument(getDocumentName(), "resetEdit()");

    return true;
}

void TaskDlgPost::recompute()
{
    Gui::ViewProviderDocumentObject* vp = getView();
    if (vp) {
        vp->getObject()->getDocument()->recompute();
    }
}

void TaskDlgPost::modifyStandardButtons(QDialogButtonBox* box)
{
    if (box->button(QDialogButtonBox::Apply)) {
        box->button(QDialogButtonBox::Apply)->setDefault(true);
    }
}

void TaskDlgPost::processCollapsedWidgets()
{

    for (auto& widget : Content) {
        auto* task_box = dynamic_cast<Gui::TaskView::TaskBox*>(widget);
        if (!task_box) {
            continue;
        }
        // get the task widget and check if it is a post widget
        auto* taskwidget = task_box->groupLayout()->itemAt(0)->widget();
        auto* post_widget = dynamic_cast<TaskPostWidget*>(taskwidget);
        if (!post_widget || !post_widget->initiallyCollapsed()) {
            continue;
        }
        post_widget->setGeometry(QRect(QPoint(0, 0), post_widget->sizeHint()));
        task_box->hideGroupBox();
    }
}

// ***************************************************************************
// box to set the coloring
TaskPostDisplay::TaskPostDisplay(ViewProviderFemPostObject* view, QWidget* parent)
    : TaskPostWidget(view, Gui::BitmapFactory().pixmap("FEM_ResultShow"), QString(), parent)
    , ui(new Ui_TaskPostDisplay)
{
    // setup the ui
    ui->setupUi(this);
    setWindowTitle(
        tr("Result display options"));  // set title here as setupUi overrides the constructor title
    setupConnections();

    // update all fields
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->DisplayMode,
                          ui->Representation);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Field, ui->Field);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Component, ui->VectorMode);

    // get Transparency from ViewProvider
    int trans = getTypedView<ViewProviderFemPostObject>()->Transparency.getValue();
    // sync the trancparency slider
    ui->Transparency->setValue(trans);
    ui->Transparency->setToolTip(QString::number(trans) + QStringLiteral(" %"));
}

TaskPostDisplay::~TaskPostDisplay() = default;

void TaskPostDisplay::setupConnections()
{
    connect(ui->Representation,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostDisplay::onRepresentationActivated);
    connect(ui->Field,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostDisplay::onFieldActivated);
    connect(ui->VectorMode,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostDisplay::onVectorModeActivated);
    connect(ui->Transparency,
            &QSlider::valueChanged,
            this,
            &TaskPostDisplay::onTransparencyValueChanged);
}

void TaskPostDisplay::slotAddedFunction()
{
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Field, ui->Field);
}

void TaskPostDisplay::onRepresentationActivated(int i)
{
    getTypedView<ViewProviderFemPostObject>()->DisplayMode.setValue(i);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Field, ui->Field);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Component, ui->VectorMode);
}

void TaskPostDisplay::onFieldActivated(int i)
{
    getTypedView<ViewProviderFemPostObject>()->Field.setValue(i);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Component, ui->VectorMode);
}

void TaskPostDisplay::onVectorModeActivated(int i)
{
    getTypedView<ViewProviderFemPostObject>()->Component.setValue(i);
}

void TaskPostDisplay::onTransparencyValueChanged(int i)
{
    getTypedView<ViewProviderFemPostObject>()->Transparency.setValue(i);
    ui->Transparency->setToolTip(QString::number(i) + QStringLiteral(" %"));
    // highlight the tooltip
    QToolTip::showText(QCursor::pos(), QString::number(i) + QStringLiteral(" %"), nullptr);
}

void TaskPostDisplay::applyPythonCode()
{}

// ***************************************************************************
// functions
TaskPostFunction::TaskPostFunction(ViewProviderFemPostFunction* view, QWidget* parent)
    : TaskPostWidget(view,
                     Gui::BitmapFactory().pixmap("fem-post-geo-plane"),
                     tr("Implicit function"),
                     parent)
{
    // we load the views widget
    FunctionWidget* w = getTypedView<ViewProviderFemPostFunction>()->createControlWidget();
    w->setParent(this);
    w->setViewProvider(getTypedView<ViewProviderFemPostFunction>());

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(w);
    setLayout(layout);
}

TaskPostFunction::~TaskPostFunction() = default;

void TaskPostFunction::applyPythonCode()
{
    // we apply the views widgets python code
}


// ***************************************************************************
// Frames
TaskPostFrames::TaskPostFrames(ViewProviderFemPostObject* view, QWidget* parent)
    : TaskPostWidget(view, Gui::BitmapFactory().pixmap("FEM_PostFrames"), QString(), parent)
    , ui(new Ui_TaskPostFrames)
{
    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Result Frames"));
    setupConnections();

    // populate the data
    auto pipeline = static_cast<Fem::FemPostPipeline*>(getObject());
    ui->Type->setText(QString::fromStdString(pipeline->getFrameType()));

    auto unit = pipeline->getFrameUnit();
    auto steps = pipeline->getFrameValues();
    for (unsigned long i = 0; i < steps.size(); i++) {
        QTableWidgetItem* idx = new QTableWidgetItem(QString::number(i));
        QTableWidgetItem* value = new QTableWidgetItem(
            QString::fromStdString(Base::Quantity(steps[i], unit).getUserString()));

        int rowIdx = ui->FrameTable->rowCount();
        ui->FrameTable->insertRow(rowIdx);
        ui->FrameTable->setItem(rowIdx, 0, idx);
        ui->FrameTable->setItem(rowIdx, 1, value);
    }
    ui->FrameTable->selectRow(pipeline->Frame.getValue());
}

TaskPostFrames::~TaskPostFrames() = default;

void TaskPostFrames::setupConnections()
{
    connect(ui->FrameTable,
            qOverload<>(&QTableWidget::itemSelectionChanged),
            this,
            &TaskPostFrames::onSelectionChanged);
}

void TaskPostFrames::onSelectionChanged()
{
    auto selection = ui->FrameTable->selectedItems();
    if (selection.count() > 0) {
        static_cast<Fem::FemPostPipeline*>(getObject())->Frame.setValue(selection.front()->row());
        recompute();
    }
}


void TaskPostFrames::applyPythonCode()
{
    // we apply the views widgets python code
}

bool TaskPostFrames::initiallyCollapsed()
{

    return (ui->FrameTable->rowCount() == 0);
}

// ***************************************************************************
// in the following, the different filters sorted alphabetically
// ***************************************************************************


// ***************************************************************************
// Branch
TaskPostBranch::TaskPostBranch(ViewProviderFemPostBranchFilter* view, QWidget* parent)
    : TaskPostWidget(view, Gui::BitmapFactory().pixmap("FEM_PostBranchFilter"), QString(), parent)
    , ui(new Ui_TaskPostBranch)
{
    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Branch behaviour"));
    setupConnections();

    // populate the data
    auto branch = static_cast<Fem::FemPostBranchFilter*>(getObject());

    ui->ModeBox->setCurrentIndex(branch->Mode.getValue());
    ui->OutputBox->setCurrentIndex(branch->Output.getValue());
}

TaskPostBranch::~TaskPostBranch() = default;

void TaskPostBranch::setupConnections()
{
    connect(ui->ModeBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPostBranch::onModeIndexChanged);

    connect(ui->OutputBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPostBranch::onOutputIndexChanged);
}

void TaskPostBranch::onModeIndexChanged(int idx)
{
    static_cast<Fem::FemPostBranchFilter*>(getObject())->Mode.setValue(idx);
}

void TaskPostBranch::onOutputIndexChanged(int idx)
{
    static_cast<Fem::FemPostBranchFilter*>(getObject())->Output.setValue(idx);
}


void TaskPostBranch::applyPythonCode()
{
    // we apply the views widgets python code
}


// ***************************************************************************
// data along line filter
TaskPostDataAlongLine::TaskPostDataAlongLine(ViewProviderFemPostDataAlongLine* view,
                                             QWidget* parent)
    : TaskPostWidget(view,
                     Gui::BitmapFactory().pixmap("FEM_PostFilterDataAlongLine"),
                     QString(),
                     parent)
    , ui(new Ui_TaskPostDataAlongLine)
    , marker(nullptr)
{
    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Data along a line options"));
    setupConnectionsStep1();

    QSize size = ui->point1X->sizeForText(QStringLiteral("000000000000"));
    ui->point1X->setMinimumWidth(size.width());
    ui->point1Y->setMinimumWidth(size.width());
    ui->point1Z->setMinimumWidth(size.width());
    ui->point2X->setMinimumWidth(size.width());
    ui->point2Y->setMinimumWidth(size.width());
    ui->point2Z->setMinimumWidth(size.width());

    // set decimals before the edits are filled to avoid rounding mistakes
    int UserDecimals = Base::UnitsApi::getDecimals();
    ui->point1X->setDecimals(UserDecimals);
    ui->point1Y->setDecimals(UserDecimals);
    ui->point1Z->setDecimals(UserDecimals);
    ui->point2X->setDecimals(UserDecimals);
    ui->point2Y->setDecimals(UserDecimals);
    ui->point2Z->setDecimals(UserDecimals);

    Base::Unit lengthUnit = getObject<Fem::FemPostDataAlongLineFilter>()->Point1.getUnit();
    ui->point1X->setUnit(lengthUnit);
    ui->point1Y->setUnit(lengthUnit);
    ui->point1Z->setUnit(lengthUnit);
    lengthUnit = getObject<Fem::FemPostDataAlongLineFilter>()->Point2.getUnit();
    ui->point2X->setUnit(lengthUnit);
    ui->point2Y->setUnit(lengthUnit);
    ui->point2Z->setUnit(lengthUnit);

    const Base::Vector3d& vec1 = getObject<Fem::FemPostDataAlongLineFilter>()->Point1.getValue();
    ui->point1X->setValue(vec1.x);
    ui->point1Y->setValue(vec1.y);
    ui->point1Z->setValue(vec1.z);

    const Base::Vector3d& vec2 = getObject<Fem::FemPostDataAlongLineFilter>()->Point2.getValue();
    ui->point2X->setValue(vec2.x);
    ui->point2Y->setValue(vec2.y);
    ui->point2Z->setValue(vec2.z);

    int res = getObject<Fem::FemPostDataAlongLineFilter>()->Resolution.getValue();
    ui->resolution->setValue(res);

    setupConnectionsStep2();

    // update all fields
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->DisplayMode,
                          ui->Representation);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Field, ui->Field);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Component, ui->VectorMode);
}

TaskPostDataAlongLine::~TaskPostDataAlongLine()
{
    if (marker && marker->getView()) {
        marker->getView()->setEditing(false);
        marker->getView()->removeEventCallback(SoMouseButtonEvent::getClassTypeId(),
                                               pointCallback,
                                               marker);
    }
}

void TaskPostDataAlongLine::setupConnectionsStep1()
{
    connect(ui->SelectPoints,
            &QPushButton::clicked,
            this,
            &TaskPostDataAlongLine::onSelectPointsClicked);
    connect(ui->CreatePlot,
            &QPushButton::clicked,
            this,
            &TaskPostDataAlongLine::onCreatePlotClicked);
    connect(ui->Representation,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostDataAlongLine::onRepresentationActivated);
    connect(ui->Field,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostDataAlongLine::onFieldActivated);
    connect(ui->VectorMode,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostDataAlongLine::onVectorModeActivated);
}

void TaskPostDataAlongLine::setupConnectionsStep2()
{
    connect(ui->point1X,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAlongLine::point1Changed);
    connect(ui->point1Y,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAlongLine::point1Changed);
    connect(ui->point1Z,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAlongLine::point1Changed);
    connect(ui->point2X,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAlongLine::point2Changed);
    connect(ui->point2Y,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAlongLine::point2Changed);
    connect(ui->point2Z,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAlongLine::point2Changed);
    connect(ui->resolution,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            &TaskPostDataAlongLine::resolutionChanged);
}

void TaskPostDataAlongLine::applyPythonCode()
{}

static const char* cursor_triangle[] = {"32 17 3 1",
                                        "       c None",
                                        ".      c #FFFFFF",
                                        "+      c #FF0000",
                                        "      .                         ",
                                        "      .                         ",
                                        "      .                         ",
                                        "      .                         ",
                                        "      .                         ",
                                        "                                ",
                                        ".....   .....                   ",
                                        "                                ",
                                        "      .                         ",
                                        "      .                         ",
                                        "      .        ++               ",
                                        "      .       +  +              ",
                                        "      .      + ++ +             ",
                                        "            + ++++ +            ",
                                        "           +  ++ ++ +           ",
                                        "          + ++++++++ +          ",
                                        "         ++  ++  ++  ++         "};

void TaskPostDataAlongLine::onSelectPointsClicked()
{
    Gui::Command::doCommand(Gui::Command::Doc, ObjectVisible().c_str());
    auto view = static_cast<Gui::View3DInventor*>(getView()->getDocument()->getActiveView());
    if (view) {
        Gui::View3DInventorViewer* viewer = view->getViewer();
        viewer->setEditing(true);
        viewer->setEditingCursor(QCursor(QPixmap(cursor_triangle), 7, 7));

        if (!marker) {
            // Derives from QObject and we have a parent object, so we don't
            // require a delete.
            auto obj = getObject<Fem::FemPostDataAlongLineFilter>();
            marker = new DataAlongLineMarker(viewer, obj);
            marker->setParent(this);
        }
        else if (marker->countPoints()) {
            marker->clearPoints();
        }

        if (!marker->connSelectPoint) {
            viewer->addEventCallback(SoMouseButtonEvent::getClassTypeId(),
                                     TaskPostDataAlongLine::pointCallback,
                                     marker);
            marker->connSelectPoint = connect(marker,
                                              &DataAlongLineMarker::PointsChanged,
                                              this,
                                              &TaskPostDataAlongLine::onChange);
        }
    }
}

std::string TaskPostDataAlongLine::ObjectVisible()
{
    return "for amesh in App.activeDocument().Objects:\n\
    if \"Mesh\" in amesh.TypeId:\n\
         aparttoshow = amesh.Name.replace(\"_Mesh\",\"\")\n\
         for apart in App.activeDocument().Objects:\n\
             if aparttoshow == apart.Name:\n\
                 apart.ViewObject.Visibility = True\n";
}

void TaskPostDataAlongLine::onCreatePlotClicked()
{
    App::DocumentObjectT objT(getObject());
    std::string ObjName = objT.getObjectPython();
    Gui::doCommandT(Gui::Command::Doc, "x = %s.XAxisData", ObjName);
    Gui::doCommandT(Gui::Command::Doc, "y = %s.YAxisData", ObjName);
    Gui::doCommandT(Gui::Command::Doc, "title = %s.PlotData", ObjName);
    Gui::doCommandT(Gui::Command::Doc, Plot().c_str());
    recompute();
}

void TaskPostDataAlongLine::onChange(double x1,
                                     double y1,
                                     double z1,
                                     double x2,
                                     double y2,
                                     double z2)
{
    // call point1Changed only once
    ui->point1X->blockSignals(true);
    ui->point1Y->blockSignals(true);
    ui->point1Z->blockSignals(true);
    ui->point1X->setValue(x1);
    ui->point1Y->setValue(y1);
    ui->point1Z->setValue(z1);
    ui->point1X->blockSignals(false);
    ui->point1Y->blockSignals(false);
    ui->point1Z->blockSignals(false);
    point1Changed(0.0);

    // same for point 2
    ui->point2X->blockSignals(true);
    ui->point2Y->blockSignals(true);
    ui->point2Z->blockSignals(true);
    ui->point2X->setValue(x2);
    ui->point2Y->setValue(y2);
    ui->point2Z->setValue(z2);
    ui->point2X->blockSignals(false);
    ui->point2Y->blockSignals(false);
    ui->point2Z->blockSignals(false);
    point2Changed(0.0);

    if (marker && marker->getView()) {
        // leave mode
        marker->getView()->setEditing(false);
        marker->getView()->removeEventCallback(SoMouseButtonEvent::getClassTypeId(),
                                               TaskPostDataAlongLine::pointCallback,
                                               marker);
        disconnect(marker->connSelectPoint);
    }
}

void TaskPostDataAlongLine::point1Changed(double)
{
    try {
        SbVec3f vec(ui->point1X->value().getValue(),
                    ui->point1Y->value().getValue(),
                    ui->point1Z->value().getValue());
        std::string ObjName = getObject()->getNameInDocument();
        Gui::cmdAppDocumentArgs(getDocument(),
                                "%s.Point1 = App.Vector(%f, %f, %f)",
                                ObjName,
                                vec[0],
                                vec[1],
                                vec[2]);

        if (marker && marker->countPoints() > 0) {
            marker->setPoint(0, vec);
        }

        // recompute the feature to fill all fields with data at this point
        getObject()->recomputeFeature();
        // refresh the color bar range
        auto currentField = getTypedView<ViewProviderFemPostObject>()->Field.getValue();
        getTypedView<ViewProviderFemPostObject>()->Field.setValue(currentField);
        // also the axis data must be refreshed to get correct plots
        getObject<Fem::FemPostDataAlongLineFilter>()->GetAxisData();
    }
    catch (const Base::Exception& e) {
        e.reportException();
    }
}

void TaskPostDataAlongLine::point2Changed(double)
{
    try {
        SbVec3f vec(ui->point2X->value().getValue(),
                    ui->point2Y->value().getValue(),
                    ui->point2Z->value().getValue());
        std::string ObjName = getObject()->getNameInDocument();
        Gui::cmdAppDocumentArgs(getDocument(),
                                "%s.Point2 = App.Vector(%f, %f, %f)",
                                ObjName,
                                vec[0],
                                vec[1],
                                vec[2]);

        if (marker && marker->countPoints() > 1) {
            marker->setPoint(1, vec);
        }

        // recompute the feature to fill all fields with data at this point
        getObject()->recomputeFeature();
        // refresh the color bar range
        auto currentField = getTypedView<ViewProviderFemPostObject>()->Field.getValue();
        getTypedView<ViewProviderFemPostObject>()->Field.setValue(currentField);
        // also the axis data must be refreshed to get correct plots
        getObject<Fem::FemPostDataAlongLineFilter>()->GetAxisData();
    }
    catch (const Base::Exception& e) {
        e.what();
    }
}

void TaskPostDataAlongLine::resolutionChanged(int val)
{
    getObject<Fem::FemPostDataAlongLineFilter>()->Resolution.setValue(val);
    // recompute the feature
    getObject()->recomputeFeature();
    // axis data must be refreshed
    getObject<Fem::FemPostDataAlongLineFilter>()->GetAxisData();
    // eventually a full recompute is necessary
    getView()->getObject()->getDocument()->recompute();
}

void TaskPostDataAlongLine::pointCallback(void* ud, SoEventCallback* n)
{
    const SoMouseButtonEvent* mbe = static_cast<const SoMouseButtonEvent*>(n->getEvent());
    Gui::View3DInventorViewer* view = static_cast<Gui::View3DInventorViewer*>(n->getUserData());
    PointMarker* pm = static_cast<PointMarker*>(ud);

    // Mark all incoming mouse button events as handled, especially,
    // to deactivate the selection node
    n->getAction()->setHandled();

    if (mbe->getButton() == SoMouseButtonEvent::BUTTON1 && mbe->getState() == SoButtonEvent::DOWN) {
        const SoPickedPoint* point = n->getPickedPoint();
        if (!point) {
            Base::Console().message("No point picked.\n");
            return;
        }

        n->setHandled();
        if (pm->countPoints() < 2) {
            pm->addPoint(point->getPoint());
        }

        if (pm->countPoints() == 2) {
            QEvent* e = new QEvent(QEvent::User);
            QApplication::postEvent(pm, e);
        }
    }
    else if (mbe->getButton() == SoMouseButtonEvent::BUTTON2
             && mbe->getState() == SoButtonEvent::UP) {
        n->setHandled();
        view->setEditing(false);
        view->removeEventCallback(SoMouseButtonEvent::getClassTypeId(),
                                  TaskPostDataAlongLine::pointCallback,
                                  ud);
        disconnect(pm->connSelectPoint);
    }
}

void TaskPostDataAlongLine::onRepresentationActivated(int i)
{
    getTypedView<ViewProviderFemPostObject>()->DisplayMode.setValue(i);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Field, ui->Field);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Component, ui->VectorMode);
}

void TaskPostDataAlongLine::onFieldActivated(int i)
{
    getTypedView<ViewProviderFemPostObject>()->Field.setValue(i);
    std::string FieldName = ui->Field->currentText().toStdString();
    getObject<Fem::FemPostDataAlongLineFilter>()->PlotData.setValue(FieldName);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Component, ui->VectorMode);

    auto vecMode = static_cast<ViewProviderFemPostObject*>(getView())->Component.getEnum();
    getObject<Fem::FemPostDataAlongLineFilter>()->PlotDataComponent.setValue(vecMode);
}

void TaskPostDataAlongLine::onVectorModeActivated(int i)
{
    getTypedView<ViewProviderFemPostObject>()->Component.setValue(i);
    int comp = ui->VectorMode->currentIndex();
    getObject<Fem::FemPostDataAlongLineFilter>()->PlotDataComponent.setValue(comp);
}

std::string TaskPostDataAlongLine::Plot()
{
    auto obj = getObject<Fem::FemPostDataAlongLineFilter>();
    std::string yLabel;
    // if there is only one component, it is the magnitude
    if (obj->PlotDataComponent.getEnum().maxValue() < 1) {
        yLabel = "Magnitude";
    }
    else {
        yLabel = obj->PlotDataComponent.getValueAsString();
    }

    auto xlabel = tr("Length", "X-Axis plot label");
    std::ostringstream oss;
    oss << "import FreeCAD\n\
from PySide import QtCore\n\
import numpy as np\n\
from matplotlib import pyplot as plt\n\
plt.ioff()\n\
plt.figure(title)\n\
plt.plot(x, y)\n\
plt.xlabel(\""
        << xlabel.toStdString() << "\")\n\
plt.ylabel(\""
        << yLabel << "\")\n\
plt.title(title)\n\
plt.grid()\n\
fig_manager = plt.get_current_fig_manager()\n\
fig_manager.window.setParent(FreeCADGui.getMainWindow())\n\
fig_manager.window.setWindowFlag(QtCore.Qt.Tool)\n\
plt.show()\n";

    return oss.str();
}


// ***************************************************************************
// data at point filter
TaskPostDataAtPoint::TaskPostDataAtPoint(ViewProviderFemPostDataAtPoint* view, QWidget* parent)
    : TaskPostWidget(view,
                     Gui::BitmapFactory().pixmap("FEM_PostFilterDataAtPoint"),
                     QString(),
                     parent)
    , viewer(nullptr)
    , connSelectPoint(QMetaObject::Connection())
    , ui(new Ui_TaskPostDataAtPoint)
{
    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Data at point options"));
    setupConnections();

    QSize size = ui->centerX->sizeForText(QStringLiteral("000000000000"));
    ui->centerX->setMinimumWidth(size.width());
    ui->centerY->setMinimumWidth(size.width());
    ui->centerZ->setMinimumWidth(size.width());

    // set decimals before the edits are filled to avoid rounding mistakes
    int UserDecimals = Base::UnitsApi::getDecimals();
    ui->centerX->setDecimals(UserDecimals);
    ui->centerY->setDecimals(UserDecimals);
    ui->centerZ->setDecimals(UserDecimals);

    const Base::Unit lengthUnit = getObject<Fem::FemPostDataAtPointFilter>()->Center.getUnit();
    ui->centerX->setUnit(lengthUnit);
    ui->centerY->setUnit(lengthUnit);
    ui->centerZ->setUnit(lengthUnit);

    const Base::Vector3d& vec = getObject<Fem::FemPostDataAtPointFilter>()->Center.getValue();
    ui->centerX->setValue(vec.x);
    ui->centerY->setValue(vec.y);
    ui->centerZ->setValue(vec.z);

    // update all fields
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Field, ui->Field);

    // read in point value
    auto pointValue = getObject<Fem::FemPostDataAtPointFilter>()->PointData[0];
    showValue(pointValue, getObject<Fem::FemPostDataAtPointFilter>()->Unit.getValue());

    connect(ui->centerX,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAtPoint::centerChanged);
    connect(ui->centerY,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAtPoint::centerChanged);
    connect(ui->centerZ,
            qOverload<double>(&Gui::QuantitySpinBox::valueChanged),
            this,
            &TaskPostDataAtPoint::centerChanged);

    // the point filter object needs to be recomputed
    // to fill all fields with data at the current point
    getObject()->recomputeFeature();
}

TaskPostDataAtPoint::~TaskPostDataAtPoint()
{
    App::Document* doc = getDocument();
    if (doc) {
        doc->recompute();
    }
    if (viewer) {
        viewer->setEditing(false);
        viewer->removeEventCallback(SoMouseButtonEvent::getClassTypeId(), pointCallback, this);
    }
}

void TaskPostDataAtPoint::setupConnections()
{
    connect(ui->SelectPoint,
            &QPushButton::clicked,
            this,
            &TaskPostDataAtPoint::onSelectPointClicked);
    connect(ui->Field,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostDataAtPoint::onFieldActivated);
}

void TaskPostDataAtPoint::applyPythonCode()
{}

void TaskPostDataAtPoint::onSelectPointClicked()
{
    Gui::Command::doCommand(Gui::Command::Doc, objectVisible(true).c_str());
    auto view = static_cast<Gui::View3DInventor*>(getView()->getDocument()->getActiveView());
    if (view) {
        viewer = view->getViewer();
        viewer->setEditing(true);
        viewer->setEditingCursor(QCursor(QPixmap(cursor_triangle), 7, 7));

        if (!connSelectPoint) {
            viewer->addEventCallback(SoMouseButtonEvent::getClassTypeId(),
                                     TaskPostDataAtPoint::pointCallback,
                                     this);
            connSelectPoint = connect(this,
                                      &TaskPostDataAtPoint::PointsChanged,
                                      this,
                                      &TaskPostDataAtPoint::onChange);
        }
    }
    getTypedView<ViewProviderFemPostObject>()->DisplayMode.setValue(1);
    updateEnumerationList(getTypedView<ViewProviderFemPostObject>()->Field, ui->Field);
}

std::string TaskPostDataAtPoint::objectVisible(bool visible) const
{
    std::ostringstream oss;
    std::string v = visible ? "True" : "False";
    oss << "for amesh in App.activeDocument().Objects:\n\
    if \"Mesh\" in amesh.TypeId:\n\
         aparttoshow = amesh.Name.replace(\"_Mesh\",\"\")\n\
         for apart in App.activeDocument().Objects:\n\
             if aparttoshow == apart.Name:\n\
                 apart.ViewObject.Visibility ="
        << v << "\n";

    return oss.str();
}

void TaskPostDataAtPoint::onChange(double x, double y, double z)
{
    // call centerChanged only once
    ui->centerX->blockSignals(true);
    ui->centerY->blockSignals(true);
    ui->centerZ->blockSignals(true);
    ui->centerX->setValue(x);
    ui->centerY->setValue(y);
    ui->centerZ->setValue(z);
    ui->centerX->blockSignals(false);
    ui->centerY->blockSignals(false);
    ui->centerZ->blockSignals(false);
    centerChanged(0.0);
    Gui::Command::doCommand(Gui::Command::Doc, objectVisible(false).c_str());

    if (viewer) {
        // leave mode
        viewer->setEditing(false);
        viewer->removeEventCallback(SoMouseButtonEvent::getClassTypeId(),
                                    TaskPostDataAtPoint::pointCallback,
                                    this);
        disconnect(connSelectPoint);
    }
}

void TaskPostDataAtPoint::centerChanged(double)
{
    try {
        std::string ObjName = getObject()->getNameInDocument();
        Gui::cmdAppDocumentArgs(getDocument(),
                                "%s.Center = App.Vector(%f, %f, %f)",
                                ObjName,
                                ui->centerX->value().getValue(),
                                ui->centerY->value().getValue(),
                                ui->centerZ->value().getValue());

        // recompute the feature to fill all fields with data at this point
        getObject()->recomputeFeature();
        // show the data dialog by calling on_Field_activated with the field that is currently set
        auto currentField = getTypedView<ViewProviderFemPostObject>()->Field.getValue();
        onFieldActivated(currentField);
    }
    catch (const Base::Exception& e) {
        e.reportException();
    }
}

void TaskPostDataAtPoint::pointCallback(void* ud, SoEventCallback* n)
{
    const SoMouseButtonEvent* mbe = static_cast<const SoMouseButtonEvent*>(n->getEvent());
    Gui::View3DInventorViewer* view = static_cast<Gui::View3DInventorViewer*>(n->getUserData());
    auto taskPost = static_cast<TaskPostDataAtPoint*>(ud);

    // Mark all incoming mouse button events as handled, especially,
    // to deactivate the selection node
    n->getAction()->setHandled();

    if (mbe->getButton() == SoMouseButtonEvent::BUTTON1 && mbe->getState() == SoButtonEvent::DOWN) {
        const SoPickedPoint* point = n->getPickedPoint();
        if (!point) {
            Base::Console().message("No point picked.\n");
            return;
        }

        n->setHandled();
        const SbVec3f& pt = point->getPoint();
        Q_EMIT taskPost->PointsChanged(pt[0], pt[1], pt[2]);
    }
    else if (mbe->getButton() == SoMouseButtonEvent::BUTTON2
             && mbe->getState() == SoButtonEvent::UP) {
        n->setHandled();
        view->setEditing(false);
        view->removeEventCallback(SoMouseButtonEvent::getClassTypeId(),
                                  TaskPostDataAtPoint::pointCallback,
                                  ud);
        disconnect(taskPost->connSelectPoint);
    }
}

void TaskPostDataAtPoint::onFieldActivated(int i)
{
    getTypedView<ViewProviderFemPostObject>()->Field.setValue(i);
    std::string FieldName = ui->Field->currentText().toStdString();
    // there is no "None" for the FieldName property, thus return here
    if (FieldName == "None") {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("");
        ui->ValueAtPoint->clear();
        return;
    }
    getObject<Fem::FemPostDataAtPointFilter>()->FieldName.setValue(FieldName);

    // Set the unit for the different known result types.

    //  CCX names
    if ((FieldName == "von Mises Stress") || (FieldName == "Tresca Stress")
        || (FieldName == "Major Principal Stress") || (FieldName == "Intermediate Principal Stress")
        || (FieldName == "Minor Principal Stress") || (FieldName == "Major Principal Stress Vector")
        || (FieldName == "Intermediate Principal Stress Vector")
        || (FieldName == "Minor Principal Stress Vector") || (FieldName == "Stress xx component")
        || (FieldName == "Stress xy component") || (FieldName == "Stress xz component")
        || (FieldName == "Stress yy component") || (FieldName == "Stress yz component")
        || (FieldName == "Stress zz component")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("Pa");
    }
    // The Elmer names are different. If there are EigenModes, the names are unique for
    // every mode. Therefore we only check for the beginning of the name.
    else if ((FieldName.find("tresca", 0) == 0) || (FieldName.find("vonmises", 0) == 0)
             || (FieldName.find("stress_", 0) == 0)
             || (FieldName.find("principal stress", 0) == 0)) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("Pa");
    }
    else if ((FieldName == "current density") || (FieldName == "current density re")
             || (FieldName == "current density im") || (FieldName == "current density abs")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("A/m^2");
    }
    else if ((FieldName == "Displacement") || (FieldName == "Displacement Magnitude")
             || (FieldName.find("displacement", 0) == 0)) {  // Elmer name
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("m");
    }
    else if (FieldName == "electric energy density") {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("J/m^3");
    }
    else if ((FieldName == "electric field") || (FieldName == "electric field re")
             || (FieldName == "electric field im") || (FieldName == "electric field abs")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("V/m");
    }
    else if (FieldName == "electric flux") {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("A*s/m^2");
    }
    else if (FieldName == "electric force density") {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("N/m^2");
    }
    else if ((FieldName == "harmonic loss linear") || (FieldName == "harmonic loss quadratic")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("W");
    }
    else if ((FieldName == "joule heating") || (FieldName == "nodal joule heating")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("J");
    }
    else if ((FieldName == "magnetic field strength") || (FieldName == "magnetic field strength re")
             || (FieldName == "magnetic field strength im")
             || (FieldName == "magnetic field strength abs")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("A/m");
    }
    else if ((FieldName == "magnetic flux density") || (FieldName == "magnetic flux density re")
             || (FieldName == "magnetic flux density im")
             || (FieldName == "magnetic flux density abs")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("T");
    }
    else if ((FieldName == "maxwell stress 1") || (FieldName == "maxwell stress 2")
             || (FieldName == "maxwell stress 3") || (FieldName == "maxwell stress 4")
             || (FieldName == "maxwell stress 5") || (FieldName == "maxwell stress 6")
             || (FieldName == "maxwell stress re 1") || (FieldName == "maxwell stress re 2")
             || (FieldName == "maxwell stress re 3") || (FieldName == "maxwell stress re 4")
             || (FieldName == "maxwell stress re 5") || (FieldName == "maxwell stress re 6")
             || (FieldName == "maxwell stress im 1") || (FieldName == "maxwell stress im 2")
             || (FieldName == "maxwell stress im 3") || (FieldName == "maxwell stress im 4")
             || (FieldName == "maxwell stress im 5") || (FieldName == "maxwell stress im 6")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("As/m^3");
    }
    else if (FieldName == "nodal force") {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("N");
    }
    else if ((FieldName == "potential") || (FieldName == "potential re")
             || (FieldName == "potential im") || (FieldName == "potential abs")
             || (FieldName == "av") || (FieldName == "av re") || (FieldName == "av im")
             || (FieldName == "av abs")) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("V");
    }
    else if (FieldName == "potential flux") {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("W/m^2");
    }
    // potential loads are in Coulomb: https://www.elmerfem.org/forum/viewtopic.php?t=7780
    else if (FieldName == "potential loads") {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("C");
    }
    else if (
        // CalculiX name
        FieldName == "Temperature" ||
        // Elmer name
        ((FieldName.find("temperature", 0) == 0) && (FieldName != "temperature flux"))) {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("K");
    }
    else if (FieldName == "temperature flux") {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("W/m^2");
    }
    else {
        getObject<Fem::FemPostDataAtPointFilter>()->Unit.setValue("");
    }

    auto pointValue = getObject<Fem::FemPostDataAtPointFilter>()->PointData[0];
    showValue(pointValue, getObject<Fem::FemPostDataAtPointFilter>()->Unit.getValue());
}

void TaskPostDataAtPoint::showValue(double pointValue, const char* unitStr)
{
    QString value = QString::fromStdString(toString(pointValue));
    QString unit = QString::fromUtf8(unitStr);

    ui->ValueAtPoint->setText(QStringLiteral("%1 %2").arg(value, unit));

    QString field = ui->Field->currentText();
    QString posX = ui->centerX->text();
    QString posY = ui->centerY->text();
    QString posZ = ui->centerZ->text();

    QString result = tr("%1 at (%2; %3; %4) is: %5 %6").arg(field, posX, posY, posZ, value, unit);
    Base::Console().message("%s\n", result.toUtf8().data());
}

std::string TaskPostDataAtPoint::toString(double val) const
{
    // for display we must therefore convert large and small numbers to scientific notation
    // if pointValue is in the range [1e-2, 1e+4] -> fixed notation, else scientific
    bool scientific = (val < 1e-2) || (val > 1e4);
    std::ios::fmtflags flags = scientific
        ? (std::ios::scientific | std::ios::showpoint | std::ios::showpos)
        : (std::ios::fixed | std::ios::showpoint | std::ios::showpos);
    std::stringstream valueStream;
    valueStream.precision(Base::UnitsApi::getDecimals());
    valueStream.setf(flags);
    valueStream << val;

    return valueStream.str();
}


// ***************************************************************************
// clip filter
TaskPostClip::TaskPostClip(ViewProviderFemPostClip* view,
                           App::PropertyLink* function,
                           QWidget* parent)
    : TaskPostWidget(view,
                     Gui::BitmapFactory().pixmap("FEM_PostFilterClipRegion"),
                     QString(),
                     parent)
    , ui(new Ui_TaskPostClip)
{
    assert(function);
    Q_UNUSED(function);

    fwidget = nullptr;

    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Clip region, choose implicit function"));
    setupConnections();

    // the layout for the container widget
    QVBoxLayout* layout = new QVBoxLayout();
    ui->Container->setLayout(layout);

    // fill up the combo box with possible functions
    collectImplicitFunctions();

    // add the function creation command
    Gui::CommandManager& rcCmdMgr = Gui::Application::Instance->commandManager();
    Gui::Command* cmd = rcCmdMgr.getCommandByName("FEM_PostCreateFunctions");
    if (cmd && cmd->getAction()) {
        cmd->getAction()->addTo(ui->CreateButton);
    }
    ui->CreateButton->setPopupMode(QToolButton::InstantPopup);

    // load the default values
    ui->CutCells->setChecked(getObject<Fem::FemPostClipFilter>()->CutCells.getValue());
    ui->InsideOut->setChecked(getObject<Fem::FemPostClipFilter>()->InsideOut.getValue());
}

TaskPostClip::~TaskPostClip() = default;

void TaskPostClip::setupConnections()
{
    connect(ui->CreateButton,
            &QToolButton::triggered,
            this,
            &TaskPostClip::onCreateButtonTriggered);
    connect(ui->FunctionBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPostClip::onFunctionBoxCurrentIndexChanged);
    connect(ui->InsideOut, &QCheckBox::toggled, this, &TaskPostClip::onInsideOutToggled);
    connect(ui->CutCells, &QCheckBox::toggled, this, &TaskPostClip::onCutCellsToggled);
}

void TaskPostClip::applyPythonCode()
{}

void TaskPostClip::collectImplicitFunctions()
{
    std::vector<Fem::FemPostPipeline*> pipelines;
    pipelines = getDocument()->getObjectsOfType<Fem::FemPostPipeline>();
    if (!pipelines.empty()) {
        Fem::FemPostPipeline* pipeline = pipelines.front();
        Fem::FemPostFunctionProvider* provider = pipeline->getFunctionProvider();
        if (provider) {

            ui->FunctionBox->clear();
            QStringList items;
            std::size_t currentItem = 0;
            App::DocumentObject* currentFunction =
                getObject<Fem::FemPostClipFilter>()->Function.getValue();
            const std::vector<App::DocumentObject*>& funcs = provider->Group.getValues();
            for (std::size_t i = 0; i < funcs.size(); ++i) {
                items.push_back(QString::fromLatin1(funcs[i]->getNameInDocument()));
                if (currentFunction == funcs[i]) {
                    currentItem = i;
                }
            }
            ui->FunctionBox->addItems(items);
            ui->FunctionBox->setCurrentIndex(currentItem);
        }
    }
}

void TaskPostClip::onCreateButtonTriggered(QAction*)
{
    int numFuncs = ui->FunctionBox->count();
    int currentItem = ui->FunctionBox->currentIndex();
    collectImplicitFunctions();

    // if a new function was successfully added use it
    int indexCount = ui->FunctionBox->count();
    if (indexCount > currentItem + 1) {
        ui->FunctionBox->setCurrentIndex(indexCount - 1);
    }

    // When the first function ever was added, a signal must be emitted
    if (numFuncs == 0) {
        Q_EMIT emitAddedFunction();
    }

    recompute();
}

void TaskPostClip::onFunctionBoxCurrentIndexChanged(int idx)
{
    // set the correct property
    std::vector<Fem::FemPostPipeline*> pipelines;
    pipelines = getDocument()->getObjectsOfType<Fem::FemPostPipeline>();
    if (!pipelines.empty()) {
        Fem::FemPostPipeline* pipeline = pipelines.front();
        Fem::FemPostFunctionProvider* provider = pipeline->getFunctionProvider();
        if (provider) {

            const std::vector<App::DocumentObject*>& funcs = provider->Group.getValues();
            if (idx >= 0) {
                getObject<Fem::FemPostClipFilter>()->Function.setValue(funcs[idx]);
            }
            else {
                getObject<Fem::FemPostClipFilter>()->Function.setValue(nullptr);
            }
        }
    }

    // load the correct view
    Fem::FemPostFunction* fobj = static_cast<Fem::FemPostFunction*>(
        getObject<Fem::FemPostClipFilter>()->Function.getValue());
    Gui::ViewProvider* view = nullptr;
    if (fobj) {
        view = Gui::Application::Instance->getViewProvider(fobj);
    }

    if (fwidget) {
        fwidget->deleteLater();
    }

    if (view) {
        fwidget = static_cast<FemGui::ViewProviderFemPostFunction*>(view)->createControlWidget();
        fwidget->setParent(ui->Container);
        fwidget->setViewProvider(static_cast<FemGui::ViewProviderFemPostFunction*>(view));
        ui->Container->layout()->addWidget(fwidget);
    }
    recompute();
}

void TaskPostClip::onCutCellsToggled(bool val)
{
    getObject<Fem::FemPostClipFilter>()->CutCells.setValue(val);
    recompute();
}

void TaskPostClip::onInsideOutToggled(bool val)
{
    getObject<Fem::FemPostClipFilter>()->InsideOut.setValue(val);
    recompute();
}


// ***************************************************************************
// contours filter
TaskPostContours::TaskPostContours(ViewProviderFemPostContours* view, QWidget* parent)
    : TaskPostWidget(view, Gui::BitmapFactory().pixmap("FEM_PostFilterContours"), QString(), parent)
    , ui(new Ui_TaskPostContours)
{
    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Contours filter options"));
    QMetaObject::connectSlotsByName(this);

    auto obj = getObject<Fem::FemPostContoursFilter>();

    // load filter settings
    updateEnumerationList(obj->Field, ui->fieldsCB);
    updateEnumerationList(obj->VectorMode, ui->vectorsCB);
    // for a new filter, initialize the coloring
    auto colorState = obj->NoColor.getValue();
    if (!colorState && getTypedView<ViewProviderFemPostObject>()->Field.getValue() == 0) {
        getTypedView<ViewProviderFemPostObject>()->Field.setValue(1);
    }

    ui->numberContoursSB->setValue(obj->NumberOfContours.getValue());
    ui->noColorCB->setChecked(colorState);

    auto ext = obj->getExtension<Fem::FemPostSmoothFilterExtension>();
    ui->ckb_smoothing->setChecked(ext->EnableSmoothing.getValue());
    ui->dsb_relaxation->setValue(ext->RelaxationFactor.getValue());
    ui->dsb_relaxation->setEnabled(ext->EnableSmoothing.getValue());

    // connect
    connect(ui->fieldsCB,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPostContours::onFieldsChanged);
    connect(ui->vectorsCB,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPostContours::onVectorModeChanged);
    connect(ui->numberContoursSB,
            qOverload<int>(&QSpinBox::valueChanged),
            this,
            &TaskPostContours::onNumberOfContoursChanged);
    connect(ui->noColorCB, &QCheckBox::toggled, this, &TaskPostContours::onNoColorChanged);
    connect(ui->ckb_smoothing, &QCheckBox::toggled, this, &TaskPostContours::onSmoothingChanged);
    connect(ui->dsb_relaxation,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &TaskPostContours::onRelaxationChanged);
}

TaskPostContours::~TaskPostContours() = default;

void TaskPostContours::applyPythonCode()
{}

void TaskPostContours::updateFields()
{
    // update the ViewProvider Field
    // since the ViewProvider can have another field sorting, we cannot use the same index
    if (!getObject<Fem::FemPostContoursFilter>()->NoColor.getValue()) {
        std::string objectField =
            getTypedObject<Fem::FemPostContoursFilter>()->Field.getValueAsString();
        getTypedView<ViewProviderFemPostObject>()->Field.setValue(objectField.c_str());
    }
    else {
        getTypedView<ViewProviderFemPostObject>()->Field.setValue("None");
    }
}

void TaskPostContours::onFieldsChanged(int idx)
{
    getObject<Fem::FemPostContoursFilter>()->Field.setValue(idx);

    blockVectorUpdate = true;
    updateEnumerationList(getTypedObject<Fem::FemPostContoursFilter>()->VectorMode, ui->vectorsCB);
    blockVectorUpdate = false;

    // In > 99 % of the cases the coloring should be equal to the field,
    // thus change the coloring field too. Users can override this be resetting only the coloring
    // field afterwards in the properties if really necessary.
    updateFields();

    // since a new field can be e.g. no vector while the previous one was,
    // we must also update the VectorMode
    if (!getObject<Fem::FemPostContoursFilter>()->NoColor.getValue()) {
        auto newMode = getTypedObject<Fem::FemPostContoursFilter>()->VectorMode.getValue();
        getTypedView<ViewProviderFemPostObject>()->Component.setValue(newMode);
    }
}

void TaskPostContours::onVectorModeChanged(int idx)
{
    getObject<Fem::FemPostContoursFilter>()->VectorMode.setValue(idx);
    recompute();
    if (!blockVectorUpdate) {
        // we can have the case that the previous field had VectorMode "Z" but
        // since it is a 2D field, Z is eompty thus no field is available to color
        // when the user noch goes back to e.g. "Y" we must set the Field
        // first to get the possible VectorModes of that field
        updateFields();
        // now we can set the VectorMode
        if (!getObject<Fem::FemPostContoursFilter>()->NoColor.getValue()) {
            getTypedView<ViewProviderFemPostObject>()->Component.setValue(idx);
        }
    }
}

void TaskPostContours::onNumberOfContoursChanged(int number)
{
    getObject<Fem::FemPostContoursFilter>()->NumberOfContours.setValue(number);
    recompute();
}

void TaskPostContours::onNoColorChanged(bool state)
{
    getObject<Fem::FemPostContoursFilter>()->NoColor.setValue(state);
    if (state) {
        // no color
        getTypedView<ViewProviderFemPostObject>()->Field.setValue(long(0));
    }
    else {
        // set same field
        auto currentField = getTypedObject<Fem::FemPostContoursFilter>()->Field.getValue();
        // the ViewProvider field starts with an additional entry "None",
        // therefore the desired new setting is idx + 1
        getTypedView<ViewProviderFemPostObject>()->Field.setValue(currentField + 1);
        // set the Component too
        auto currentMode = getTypedObject<Fem::FemPostContoursFilter>()->VectorMode.getValue();
        getTypedView<ViewProviderFemPostObject>()->Component.setValue(currentMode);
    }
    recompute();
}

void TaskPostContours::onSmoothingChanged(bool state)
{
    auto ext = static_cast<Fem::FemPostContoursFilter*>(getObject())
                   ->getExtension<Fem::FemPostSmoothFilterExtension>();
    ext->EnableSmoothing.setValue(state);
    ui->dsb_relaxation->setEnabled(state);
    recompute();
}

void TaskPostContours::onRelaxationChanged(double value)
{
    auto ext = static_cast<Fem::FemPostContoursFilter*>(getObject())
                   ->getExtension<Fem::FemPostSmoothFilterExtension>();
    ext->RelaxationFactor.setValue(value);
    recompute();
}

// ***************************************************************************
// cut filter
TaskPostCut::TaskPostCut(ViewProviderFemPostCut* view, App::PropertyLink* function, QWidget* parent)
    : TaskPostWidget(view,
                     Gui::BitmapFactory().pixmap("FEM_PostFilterCutFunction"),
                     QString(),
                     parent)
    , ui(new Ui_TaskPostCut)
{
    assert(function);
    Q_UNUSED(function)

    fwidget = nullptr;

    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Function cut, choose implicit function"));
    setupConnections();

    // the layout for the container widget
    QVBoxLayout* layout = new QVBoxLayout();
    ui->Container->setLayout(layout);

    // fill up the combo box with possible functions
    collectImplicitFunctions();

    // add the function creation command
    Gui::CommandManager& rcCmdMgr = Gui::Application::Instance->commandManager();
    Gui::Command* cmd = rcCmdMgr.getCommandByName("FEM_PostCreateFunctions");
    if (cmd && cmd->getAction()) {
        cmd->getAction()->addTo(ui->CreateButton);
    }
    ui->CreateButton->setPopupMode(QToolButton::InstantPopup);
}

TaskPostCut::~TaskPostCut() = default;

void TaskPostCut::setupConnections()
{
    connect(ui->CreateButton, &QToolButton::triggered, this, &TaskPostCut::onCreateButtonTriggered);
    connect(ui->FunctionBox,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPostCut::onFunctionBoxCurrentIndexChanged);
}

void TaskPostCut::applyPythonCode()
{}

void TaskPostCut::collectImplicitFunctions()
{
    std::vector<Fem::FemPostPipeline*> pipelines;
    pipelines = getDocument()->getObjectsOfType<Fem::FemPostPipeline>();
    if (!pipelines.empty()) {
        Fem::FemPostPipeline* pipeline = pipelines.front();
        Fem::FemPostFunctionProvider* provider = pipeline->getFunctionProvider();
        if (provider) {

            ui->FunctionBox->clear();
            QStringList items;
            std::size_t currentItem = 0;
            App::DocumentObject* currentFunction =
                getObject<Fem::FemPostCutFilter>()->Function.getValue();
            const std::vector<App::DocumentObject*>& funcs = provider->Group.getValues();
            for (std::size_t i = 0; i < funcs.size(); ++i) {
                items.push_back(QString::fromLatin1(funcs[i]->getNameInDocument()));
                if (currentFunction == funcs[i]) {
                    currentItem = i;
                }
            }
            ui->FunctionBox->addItems(items);
            ui->FunctionBox->setCurrentIndex(currentItem);
        }
    }
}

void TaskPostCut::onCreateButtonTriggered(QAction*)
{
    int numFuncs = ui->FunctionBox->count();
    int currentItem = ui->FunctionBox->currentIndex();
    collectImplicitFunctions();

    // if a new function was successfully added use it
    int indexCount = ui->FunctionBox->count();
    if (indexCount > currentItem + 1) {
        ui->FunctionBox->setCurrentIndex(indexCount - 1);
    }

    // When the first function ever was added, a signal must be emitted
    if (numFuncs == 0) {
        Q_EMIT emitAddedFunction();
    }

    recompute();
}

void TaskPostCut::onFunctionBoxCurrentIndexChanged(int idx)
{
    // set the correct property
    std::vector<Fem::FemPostPipeline*> pipelines;
    pipelines = getDocument()->getObjectsOfType<Fem::FemPostPipeline>();
    if (!pipelines.empty()) {
        Fem::FemPostPipeline* pipeline = pipelines.front();
        Fem::FemPostFunctionProvider* provider = pipeline->getFunctionProvider();
        if (provider) {

            const std::vector<App::DocumentObject*>& funcs = provider->Group.getValues();
            if (idx >= 0) {
                getObject<Fem::FemPostCutFilter>()->Function.setValue(funcs[idx]);
            }
            else {
                getObject<Fem::FemPostCutFilter>()->Function.setValue(nullptr);
            }
        }
    }

    // load the correct view
    Fem::FemPostFunction* fobj =
        static_cast<Fem::FemPostFunction*>(getObject<Fem::FemPostCutFilter>()->Function.getValue());
    Gui::ViewProvider* view = nullptr;
    if (fobj) {
        view = Gui::Application::Instance->getViewProvider(fobj);
    }

    if (fwidget) {
        fwidget->deleteLater();
    }

    if (view) {
        fwidget = static_cast<FemGui::ViewProviderFemPostFunction*>(view)->createControlWidget();
        fwidget->setParent(ui->Container);
        fwidget->setViewProvider(static_cast<FemGui::ViewProviderFemPostFunction*>(view));
        ui->Container->layout()->addWidget(fwidget);
    }
    recompute();
}


// ***************************************************************************
// scalar clip filter
TaskPostScalarClip::TaskPostScalarClip(ViewProviderFemPostScalarClip* view, QWidget* parent)
    : TaskPostWidget(view,
                     Gui::BitmapFactory().pixmap("FEM_PostFilterClipScalar"),
                     QString(),
                     parent)
    , ui(new Ui_TaskPostScalarClip)
{
    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Scalar clip options"));
    setupConnections();

    // load the default values
    updateEnumerationList(getTypedObject<Fem::FemPostScalarClipFilter>()->Scalars, ui->Scalar);
    ui->InsideOut->setChecked(getObject<Fem::FemPostScalarClipFilter>()->InsideOut.getValue());
    App::PropertyFloatConstraint& scalar_prop = getObject<Fem::FemPostScalarClipFilter>()->Value;
    double scalar_factor = scalar_prop.getValue();

    // set spinbox scalar_factor, don't forget to sync the slider
    ui->Value->blockSignals(true);
    ui->Value->setValue(scalar_factor);
    ui->Value->blockSignals(false);

    // sync the slider
    // slider min = 0%, slider max = 100%
    //
    //                 scalar_factor
    // slider_value = --------------- x 100
    //                      max
    //
    double max = scalar_prop.getConstraints()->UpperBound;
    int slider_value = (scalar_factor / max) * 100.;
    ui->Slider->blockSignals(true);
    ui->Slider->setValue(slider_value);
    ui->Slider->blockSignals(false);
    Base::Console().log("init: scalar_factor, slider_value: %f, %i: \n",
                        scalar_factor,
                        slider_value);
}

TaskPostScalarClip::~TaskPostScalarClip() = default;

void TaskPostScalarClip::setupConnections()
{
    connect(ui->Slider, &QSlider::valueChanged, this, &TaskPostScalarClip::onSliderValueChanged);
    connect(ui->Value,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &TaskPostScalarClip::onValueValueChanged);
    connect(ui->Scalar,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPostScalarClip::onScalarCurrentIndexChanged);
    connect(ui->InsideOut, &QCheckBox::toggled, this, &TaskPostScalarClip::onInsideOutToggled);
}

void TaskPostScalarClip::applyPythonCode()
{}

void TaskPostScalarClip::onScalarCurrentIndexChanged(int idx)
{
    getObject<Fem::FemPostScalarClipFilter>()->Scalars.setValue(idx);
    recompute();

    // update constraints and values
    App::PropertyFloatConstraint& scalar_prop = getObject<Fem::FemPostScalarClipFilter>()->Value;
    double scalar_factor = scalar_prop.getValue();
    double min = scalar_prop.getConstraints()->LowerBound;
    double max = scalar_prop.getConstraints()->UpperBound;

    ui->Maximum->setText(QString::number(min));
    ui->Minimum->setText(QString::number(max));

    // set scalar_factor, don't forget to sync the slider
    ui->Value->blockSignals(true);
    ui->Value->setValue(scalar_factor);
    ui->Value->blockSignals(false);

    // sync the slider
    ui->Slider->blockSignals(true);
    int slider_value = (scalar_factor / max) * 100.;
    ui->Slider->setValue(slider_value);
    ui->Slider->blockSignals(false);
}

void TaskPostScalarClip::onSliderValueChanged(int v)
{
    App::PropertyFloatConstraint& value = getObject<Fem::FemPostScalarClipFilter>()->Value;
    double val = value.getConstraints()->LowerBound * (1 - double(v) / 100.)
        + double(v) / 100. * value.getConstraints()->UpperBound;

    value.setValue(val);
    recompute();

    // don't forget to sync the spinbox
    ui->Value->blockSignals(true);
    ui->Value->setValue(val);
    ui->Value->blockSignals(false);
}

void TaskPostScalarClip::onValueValueChanged(double v)
{
    App::PropertyFloatConstraint& value = getObject<Fem::FemPostScalarClipFilter>()->Value;
    value.setValue(v);
    recompute();

    // don't forget to sync the slider
    ui->Slider->blockSignals(true);
    ui->Slider->setValue(
        int(((v - value.getConstraints()->LowerBound)
             / (value.getConstraints()->UpperBound - value.getConstraints()->LowerBound))
            * 100.));
    ui->Slider->blockSignals(false);
}

void TaskPostScalarClip::onInsideOutToggled(bool val)
{
    getObject<Fem::FemPostScalarClipFilter>()->InsideOut.setValue(val);
    recompute();
}


// ***************************************************************************
// warp vector filter
TaskPostWarpVector::TaskPostWarpVector(ViewProviderFemPostWarpVector* view, QWidget* parent)
    : TaskPostWidget(view, Gui::BitmapFactory().pixmap("FEM_PostFilterWarp"), QString(), parent)
    , ui(new Ui_TaskPostWarpVector)
{
    // setup the ui
    ui->setupUi(this);
    setWindowTitle(tr("Warp options"));
    setupConnections();

    // load the default values for warp display
    updateEnumerationList(getTypedObject<Fem::FemPostWarpVectorFilter>()->Vector, ui->Vector);
    double warp_factor = getObject<Fem::FemPostWarpVectorFilter>()
                             ->Factor.getValue();  // get the standard warp factor

    // set spinbox warp_factor, don't forget to sync the slider
    ui->Value->blockSignals(true);
    ui->Value->setValue(warp_factor);
    ui->Value->blockSignals(false);

    // set min and max, don't forget to sync the slider
    // TODO if warp is set to standard 1.0, find a smarter way for standard min, max
    // and warp_factor may be depend on grid boundbox and min max vector values
    ui->Max->blockSignals(true);
    ui->Max->setValue(warp_factor == 0 ? 1 : warp_factor * 10.);
    ui->Max->blockSignals(false);
    ui->Min->blockSignals(true);
    ui->Min->setValue(warp_factor == 0 ? 0 : warp_factor / 10.);
    ui->Min->blockSignals(false);

    // sync slider
    ui->Slider->blockSignals(true);
    // slider min = 0%, slider max = 100%
    //
    //                 ( warp_factor - min )
    // slider_value = ----------------------- x 100
    //                     ( max - min )
    //
    int slider_value =
        (warp_factor - ui->Min->value()) / (ui->Max->value() - ui->Min->value()) * 100.;
    ui->Slider->setValue(slider_value);
    ui->Slider->blockSignals(false);
    Base::Console().log("init: warp_factor, slider_value: %f, %i: \n", warp_factor, slider_value);
}

TaskPostWarpVector::~TaskPostWarpVector() = default;

void TaskPostWarpVector::setupConnections()
{
    connect(ui->Slider, &QSlider::valueChanged, this, &TaskPostWarpVector::onSliderValueChanged);
    connect(ui->Value,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &TaskPostWarpVector::onValueValueChanged);
    connect(ui->Max,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &TaskPostWarpVector::onMaxValueChanged);
    connect(ui->Min,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &TaskPostWarpVector::onMinValueChanged);
    connect(ui->Vector,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &TaskPostWarpVector::onVectorCurrentIndexChanged);
}

void TaskPostWarpVector::applyPythonCode()
{}

void TaskPostWarpVector::onVectorCurrentIndexChanged(int idx)
{
    // combobox to choose the result to warp
    getObject<Fem::FemPostWarpVectorFilter>()->Vector.setValue(idx);
    recompute();
}

void TaskPostWarpVector::onSliderValueChanged(int slider_value)
{
    // slider changed, change warp factor and sync spinbox

    //
    //                                       ( max - min )
    // warp_factor = min + ( slider_value x --------------- )
    //                                            100
    //
    double warp_factor =
        ui->Min->value() + ((ui->Max->value() - ui->Min->value()) / 100.) * slider_value;
    getObject<Fem::FemPostWarpVectorFilter>()->Factor.setValue(warp_factor);
    recompute();

    // sync the spinbox
    ui->Value->blockSignals(true);
    ui->Value->setValue(warp_factor);
    ui->Value->blockSignals(false);
    Base::Console().log("Change: warp_factor, slider_value: %f, %i: \n", warp_factor, slider_value);
}

void TaskPostWarpVector::onValueValueChanged(double warp_factor)
{
    // spinbox changed, change warp factor and sync slider

    // TODO warp factor should not be smaller than min and greater than max,
    // but problems on automate change of warp_factor, see on_Max_valueChanged
    getObject<Fem::FemPostWarpVectorFilter>()->Factor.setValue(warp_factor);
    recompute();

    // sync the slider, see above for formula
    ui->Slider->blockSignals(true);
    int slider_value =
        (warp_factor - ui->Min->value()) / (ui->Max->value() - ui->Min->value()) * 100.;
    ui->Slider->setValue(slider_value);
    ui->Slider->blockSignals(false);
    Base::Console().log("Change: warp_factor, slider_value: %f, %i: \n", warp_factor, slider_value);
}

void TaskPostWarpVector::onMaxValueChanged(double)
{
    // TODO max should be greater than min, see a few lines later on problem on input characters
    ui->Slider->blockSignals(true);
    ui->Slider->setValue((ui->Value->value() - ui->Min->value())
                         / (ui->Max->value() - ui->Min->value()) * 100.);
    ui->Slider->blockSignals(false);

    /*
     * problem, if warp_factor is 2000 one would like to input 4000 as max, one starts to input 4
     * immediately the warp_factor is changed to 4 because 4 < 2000, but one has just input
     * one character of their 4000. * I do not know how to solve this, but the code to set slider
     * and spinbox is fine thus I leave it ...
     *
     * mhh it works if "apply changes to pipeline directly" button is deactivated,
     * still it really confuses if the button is active. More investigation is needed.
     *
    // set warp factor to max, if warp factor > max
    if (ui->Value->value() > ui->Max->value()) {
        double warp_factor = ui->Max->value();
        getObject<Fem::FemPostWarpVectorFilter>()->Factor.setValue(warp_factor);
        recompute();

        // sync the slider, see above for formula
        ui->Slider->blockSignals(true);
        int slider_value = (warp_factor - ui->Min->value())
                           / (ui->Max->value() - ui->Min->value()) * 100.;
        ui->Slider->setValue(slider_value);
        ui->Slider->blockSignals(false);
        // sync the spinbox, see above for formula
        ui->Value->blockSignals(true);
        ui->Value->setValue(warp_factor);
        ui->Value->blockSignals(false);
    Base::Console().log("Change: warp_factor, slider_value: %f, %i: \n", warp_factor, slider_value);
    }
    */
}

void TaskPostWarpVector::onMinValueChanged(double)
{
    // TODO min should be smaller than max
    // TODO if warp factor is smaller than min, warp factor should be min, don't forget to sync
    ui->Slider->blockSignals(true);
    ui->Slider->setValue((ui->Value->value() - ui->Min->value())
                         / (ui->Max->value() - ui->Min->value()) * 100.);
    ui->Slider->blockSignals(false);
}


// ***************************************************************************
// calculator filter
static const std::vector<std::string> calculatorOperators = {
    "+",   "-",   "*",    "/",    "-",    "^",    "abs",   "cos", "sin", "tan", "exp",
    "log", "pow", "sqrt", "iHat", "jHat", "kHat", "cross", "dot", "mag", "norm"};

TaskPostCalculator::TaskPostCalculator(ViewProviderFemPostCalculator* view, QWidget* parent)
    : TaskPostWidget(view,
                     Gui::BitmapFactory().pixmap("FEM_PostFilterCalculator"),
                     tr("Calculator options"),
                     parent)
    , ui(new Ui_TaskPostCalculator)
{
    // we load the views widget
    ui->setupUi(this);
    setupConnections();

    // load the default values
    auto obj = getObject<Fem::FemPostCalculatorFilter>();
    ui->let_field_name->blockSignals(true);
    ui->let_field_name->setText(QString::fromUtf8(obj->FieldName.getValue()));
    ui->let_field_name->blockSignals(false);

    ui->let_function->blockSignals(true);
    ui->let_function->setText(QString::fromUtf8(obj->Function.getValue()));
    ui->let_function->blockSignals(false);

    ui->ckb_replace_invalid->setChecked(obj->ReplaceInvalid.getValue());
    ui->dsb_replacement_value->setEnabled(obj->ReplaceInvalid.getValue());
    ui->dsb_replacement_value->setValue(obj->ReplacementValue.getValue());
    ui->dsb_replacement_value->setMaximum(std::numeric_limits<double>::max());
    ui->dsb_replacement_value->setMinimum(std::numeric_limits<double>::lowest());

    // fill available fields
    for (const auto& f : obj->getScalarVariables()) {
        ui->cb_scalars->addItem(QString::fromStdString(f));
    }
    for (const auto& f : obj->getVectorVariables()) {
        ui->cb_vectors->addItem(QString::fromStdString(f));
    }

    QStringList qOperators;
    for (const auto& o : calculatorOperators) {
        qOperators << QString::fromStdString(o);
    }
    ui->cb_operators->addItems(qOperators);

    ui->cb_scalars->setCurrentIndex(-1);
    ui->cb_vectors->setCurrentIndex(-1);
    ui->cb_operators->setCurrentIndex(-1);
}

TaskPostCalculator::~TaskPostCalculator() = default;

void TaskPostCalculator::setupConnections()
{
    connect(ui->dsb_replacement_value,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &TaskPostCalculator::onReplacementValueChanged);
    connect(ui->ckb_replace_invalid,
            &QCheckBox::toggled,
            this,
            &TaskPostCalculator::onReplaceInvalidChanged);
    connect(ui->cb_scalars,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostCalculator::onScalarsActivated);
    connect(ui->cb_vectors,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostCalculator::onVectorsActivated);
    connect(ui->cb_operators,
            qOverload<int>(&QComboBox::activated),
            this,
            &TaskPostCalculator::onOperatorsActivated);
}

void TaskPostCalculator::onReplaceInvalidChanged(bool state)
{
    auto obj = static_cast<Fem::FemPostCalculatorFilter*>(getObject());
    obj->ReplaceInvalid.setValue(state);
    ui->dsb_replacement_value->setEnabled(state);
    recompute();
}

void TaskPostCalculator::onReplacementValueChanged(double value)
{
    auto obj = static_cast<Fem::FemPostCalculatorFilter*>(getObject());
    obj->ReplacementValue.setValue(value);
    recompute();
}

void TaskPostCalculator::onScalarsActivated(int index)
{
    QString item = ui->cb_scalars->itemText(index);
    ui->let_function->insert(item);
}

void TaskPostCalculator::onVectorsActivated(int index)
{
    QString item = ui->cb_vectors->itemText(index);
    ui->let_function->insert(item);
}

void TaskPostCalculator::onOperatorsActivated(int index)
{
    QString item = ui->cb_operators->itemText(index);
    ui->let_function->insert(item);
}

void TaskPostCalculator::apply()
{
    auto obj = getObject<Fem::FemPostCalculatorFilter>();
    std::string function = ui->let_function->text().toStdString();
    std::string name = ui->let_field_name->text().toStdString();
    obj->Function.setValue(function);
    obj->FieldName.setValue(name);
    recompute();

    auto view = getTypedView<ViewProviderFemPostCalculator>();
    view->Field.setValue(obj->FieldName.getValue());
}

#include "moc_TaskPostBoxes.cpp"
