#include "app/MainWindow.h"
#include "app/ActivityBarButton.h"
#include "app/CollapsibleSplitter.h"
#include "app/LandingButton.h"
#include "ui/AutoResizingTextEdit.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QShortcut>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <algorithm>

namespace {
constexpr int kActivityBarWidth = 53;
constexpr int kSidePanelDefaultWidth = 240;
constexpr int kSidePanelMinWidth = 220;
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      editorConfigurator_(new EditorConfigurator(this)),
      executionController_(new ExecutionController(this)),
      baseAppFont_(qApp->font()) {
    themeManager_.apply(qApp, uiScale_);
    setupUi();
    setupZoomShortcuts();
    applyUiZoom();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi() {
    resize(1200, 800);
    setWindowTitle("CF Dojo - v1.0");

    // Build the stacked widget for page navigation
    mainStack_ = new QStackedWidget(this);
    mainStack_->setContentsMargins(0, 0, 0, 0);
    mainStack_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    setupLandingPage();
    setupMenuBar();
    setupMainEditor();

    // Add pages to stack
    mainStack_->addWidget(landingPage_);
    mainStack_->addWidget(mainSplitter_);
    mainStack_->setCurrentWidget(landingPage_);

    // Menu visibility follows current page
    connect(mainStack_, &QStackedWidget::currentChanged, this, [this]() {
        if (menuBar_) {
            menuBar_->setVisible(mainStack_->currentWidget() != landingPage_);
        }
    });
    if (menuBar_) {
        menuBar_->setVisible(false);
    }

    setCentralWidget(mainStack_);
}

void MainWindow::setupActivityBar() {
    activityBar_ = new QWidget();
    activityBar_->setObjectName("ActivityBar");
    activityBar_->setFixedWidth(kActivityBarWidth);

    auto *layout = new QVBoxLayout(activityBar_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Sidebar toggle at top
    sidebarToggle_ = new ActivityBarButton(":/images/testcase.svg", activityBar_);
    sidebarToggle_->setObjectName("SidebarToggle");
    sidebarToggle_->setFixedHeight(kActivityBarWidth);
    sidebarToggle_->setToolTip("Toggle side bar");
    sidebarToggle_->setCheckable(true);
    sidebarToggle_->setChecked(true);

    connect(sidebarToggle_, &QPushButton::clicked, this, [this]() {
        if (mainSplitter_) {
            mainSplitter_->toggleCollapse();
        }
    });

    // New file button
    newFileButton_ = new ActivityBarButton(":/images/file.svg", activityBar_);
    newFileButton_->setObjectName("NewFileButton");
    newFileButton_->setFixedHeight(kActivityBarWidth);
    newFileButton_->setToolTip("New File");

    layout->addWidget(sidebarToggle_, 0, Qt::AlignTop);
    layout->addWidget(newFileButton_, 0, Qt::AlignTop);
    layout->addStretch();

    // Bottom buttons container
    auto *bottomPanel = new QWidget(activityBar_);
    auto *bottomLayout = new QVBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    settingsButton_ = new ActivityBarButton(":/images/settings.svg", bottomPanel);
    settingsButton_->setObjectName("SettingsButton");
    settingsButton_->setFixedHeight(kActivityBarWidth);
    settingsButton_->setToolTip("Settings");

    backButton_ = new ActivityBarButton(":/images/arrow-left.svg", bottomPanel);
    backButton_->setObjectName("BackButton");
    backButton_->setFixedHeight(kActivityBarWidth);
    backButton_->setToolTip("Back");

    connect(backButton_, &QPushButton::clicked, this, [this]() {
        if (mainStack_ && landingPage_) {
            mainStack_->setCurrentWidget(landingPage_);
        }
    });

    bottomLayout->addWidget(settingsButton_);
    bottomLayout->addWidget(backButton_);
    layout->addWidget(bottomPanel, 0, Qt::AlignBottom);
}

void MainWindow::setupLandingPage() {
    landingPage_ = new QWidget();
    landingPage_->setObjectName("LandingPage");
    landingPage_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QHBoxLayout(landingPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    trainingButton_ = new LandingButton("Training", ":/images/train.svg", landingPage_);
    trainingButton_->setObjectName("TrainingChoice");

    contestButton_ = new LandingButton("Contest", ":/images/battle.svg", landingPage_);
    contestButton_->setObjectName("ContestChoice");

    connect(trainingButton_, &LandingButton::clicked, this, [this]() {
        if (mainStack_ && mainSplitter_) {
            mainStack_->setCurrentWidget(mainSplitter_);
        }
    });

    connect(contestButton_, &LandingButton::clicked, this, [this]() {
        if (mainStack_ && mainSplitter_) {
            mainStack_->setCurrentWidget(mainSplitter_);
        }
    });

    layout->addWidget(trainingButton_, 1);
    layout->addWidget(contestButton_, 1);
}

void MainWindow::setupMainEditor() {
    // Build editor and side panel
    const auto editorWidgets = editorConfigurator_->build(this, themeManager_);
    codeEditor_ = editorWidgets.editor;

    testPanelWidgets_ = testPanelBuilder_.build(this, this);
    sidePanel_ = testPanelWidgets_.panel;
    sidePanel_->setMinimumHeight(0);
    sidePanel_->setMinimumWidth(0);
    sidePanel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Connect test panel buttons
    if (testPanelWidgets_.addButton) {
        connect(testPanelWidgets_.addButton, &QPushButton::clicked, this, 
                &MainWindow::addTestCase);
    }
    if (testPanelWidgets_.clearCasesButton) {
        connect(testPanelWidgets_.clearCasesButton, &QPushButton::clicked, this,
                &MainWindow::clearAllTestCases);
    }

    // Create initial test case
    addTestCase();

    // Setup activity bar
    setupActivityBar();

    // Create the collapsible splitter
    mainSplitter_ = new CollapsibleSplitter(Qt::Horizontal);
    mainSplitter_->setObjectName("MainSplitter");
    mainSplitter_->setCollapsibleIndex(1);
    mainSplitter_->setMinimumPanelWidth(kSidePanelMinWidth);
    mainSplitter_->setPreferredWidth(kSidePanelDefaultWidth);

    mainSplitter_->addWidget(activityBar_);
    mainSplitter_->addWidget(sidePanel_);
    mainSplitter_->addWidget(editorWidgets.container);

    // Configure splitter behavior
    mainSplitter_->setCollapsible(0, false);  // Activity bar never collapses
    mainSplitter_->setCollapsible(1, true);   // Side panel can collapse
    mainSplitter_->setCollapsible(2, false);  // Editor never collapses

    mainSplitter_->setStretchFactor(0, 0);    // Activity bar fixed
    mainSplitter_->setStretchFactor(1, 0);    // Side panel doesn't stretch
    mainSplitter_->setStretchFactor(2, 1);    // Editor takes remaining space

    mainSplitter_->setSizes({kActivityBarWidth, kSidePanelDefaultWidth, 1});

    // Connect collapse state to toggle button
    connect(mainSplitter_, &CollapsibleSplitter::collapsedChanged,
            this, &MainWindow::onSidePanelCollapsedChanged);
}

void MainWindow::setupMenuBar() {
    menuBar_ = new QMenuBar(this);
    menuBar_->setObjectName("MainMenuBar");

    fileMenu_ = menuBar_->addMenu("File");
    fileMenu_->addAction("New");
    fileMenu_->addAction("Open...");
    fileMenu_->addAction("Save");
    fileMenu_->addSeparator();
    QAction *exitAction = fileMenu_->addAction("Exit");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    editMenu_ = menuBar_->addMenu("Edit");
    editMenu_->addAction("Undo");
    editMenu_->addAction("Redo");
    editMenu_->addSeparator();
    editMenu_->addAction("Cut");
    editMenu_->addAction("Copy");
    editMenu_->addAction("Paste");

    setMenuBar(menuBar_);
}

void MainWindow::onSidePanelCollapsedChanged(bool collapsed) {
    if (sidebarToggle_) {
        sidebarToggle_->setChecked(!collapsed);
        sidebarToggle_->setActiveState(!collapsed);
    }
}

void MainWindow::addTestCase() {
    if (!testPanelWidgets_.casesLayout || !testPanelWidgets_.casesContainer) {
        return;
    }

    const int index = static_cast<int>(caseWidgets_.size()) + 1;
    TestPanelBuilder::CaseWidgets widgets =
        testPanelBuilder_.createCase(testPanelWidgets_.casesContainer, this, index);

    // Insert before the spacer if present
    int insertIndex = testPanelWidgets_.casesLayout->count();
    if (insertIndex > 0) {
        QLayoutItem *tail = testPanelWidgets_.casesLayout->itemAt(insertIndex - 1);
        if (tail && tail->spacerItem()) {
            --insertIndex;
        }
    }
    testPanelWidgets_.casesLayout->insertWidget(insertIndex, widgets.panel);
    caseWidgets_.push_back(widgets);
    testCases_.push_back({});

    // Connect run button
    if (widgets.runButton) {
        connect(widgets.runButton, &QPushButton::clicked, this, 
                [this, button = widgets.runButton]() {
            const int caseIndex = indexForButton(button);
            if (caseIndex < 0 || caseIndex >= static_cast<int>(caseWidgets_.size())) {
                return;
            }

            TestCase &testCase = testCases_.at(static_cast<size_t>(caseIndex));
            const auto &caseWidgets = caseWidgets_.at(static_cast<size_t>(caseIndex));

            if (caseWidgets.inputEditor) {
                testCase.input = caseWidgets.inputEditor->toPlainText();
            }
            if (caseWidgets.expectedEditor) {
                testCase.expectedOutput = caseWidgets.expectedEditor->toPlainText();
            }

            executionController_->runWithBindings(makeBindings(caseWidgets));
        });
    }

    // Connect delete button
    if (widgets.deleteButton) {
        connect(widgets.deleteButton, &QPushButton::clicked, this,
                [this, button = widgets.deleteButton]() {
            const int caseIndex = indexForButton(button);
            removeTestCase(caseIndex);
        });
    }

    updateTestCaseTitles();
}

void MainWindow::removeTestCase(int index) {
    if (index < 0 || index >= static_cast<int>(caseWidgets_.size())) {
        return;
    }

    TestPanelBuilder::CaseWidgets widgets = caseWidgets_.at(static_cast<size_t>(index));
    if (widgets.panel) {
        widgets.panel->deleteLater();
    }

    caseWidgets_.erase(caseWidgets_.begin() + index);
    if (static_cast<size_t>(index) < testCases_.size()) {
        testCases_.erase(testCases_.begin() + index);
    }

    updateTestCaseTitles();
}

void MainWindow::clearAllTestCases() {
    for (auto &widgets : caseWidgets_) {
        if (widgets.panel) {
            widgets.panel->deleteLater();
        }
    }
    caseWidgets_.clear();
    testCases_.clear();
}

void MainWindow::updateTestCaseTitles() {
    for (size_t i = 0; i < caseWidgets_.size(); ++i) {
        auto *label = caseWidgets_[i].titleLabel;
        if (label) {
            label->setText(QString("Test Case %1").arg(static_cast<int>(i + 1)));
        }
    }
}

int MainWindow::indexForButton(const QPushButton *button) const {
    if (!button) {
        return -1;
    }
    for (size_t i = 0; i < caseWidgets_.size(); ++i) {
        const auto &widgets = caseWidgets_[i];
        if (widgets.runButton == button || widgets.deleteButton == button) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

ExecutionController::UiBindings MainWindow::makeBindings(
    const TestPanelBuilder::CaseWidgets &widgets) const {
    ExecutionController::UiBindings bindings;
    bindings.codeEditor = codeEditor_;
    bindings.inputEditor = widgets.inputEditor;
    bindings.expectedEditor = widgets.expectedEditor;
    bindings.outputViewer = widgets.outputViewer;
    bindings.errorViewer = widgets.errorViewer;
    bindings.statusLabel = widgets.statusLabel;
    bindings.outputSplitter = widgets.outputSplitter;
    bindings.outputBlock = widgets.outputBlock;
    bindings.errorBlock = widgets.errorBlock;
    bindings.runButton = widgets.runButton;
    return bindings;
}

void MainWindow::setupZoomShortcuts() {
    zoomInShortcut_ = new QShortcut(QKeySequence::ZoomIn, this);
    zoomOutShortcut_ = new QShortcut(QKeySequence::ZoomOut, this);
    zoomResetShortcut_ = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_0), this);

    connect(zoomInShortcut_, &QShortcut::activated, this, &MainWindow::zoomIn);
    connect(zoomOutShortcut_, &QShortcut::activated, this, &MainWindow::zoomOut);
    connect(zoomResetShortcut_, &QShortcut::activated, this, &MainWindow::resetZoom);
}

void MainWindow::applyUiZoom() {
    constexpr double kMinScale = 0.7;
    constexpr double kMaxScale = 1.8;
    uiScale_ = std::clamp(uiScale_, kMinScale, kMaxScale);

    QFont scaledFont = baseAppFont_;
    scaledFont.setPointSizeF(baseAppFont_.pointSizeF() * uiScale_);
    qApp->setFont(scaledFont);

    themeManager_.apply(qApp, uiScale_);

    if (editorConfigurator_) {
        editorConfigurator_->applyZoom(uiScale_);
    }

    testPanelBuilder_.refreshEditorSizing();
}

void MainWindow::zoomIn() {
    uiScale_ += 0.1;
    applyUiZoom();
}

void MainWindow::zoomOut() {
    uiScale_ -= 0.1;
    applyUiZoom();
}

void MainWindow::resetZoom() {
    uiScale_ = 1.0;
    applyUiZoom();
}
