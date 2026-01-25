#pragma once

#include "editor/EditorConfigurator.h"
#include "execution/ExecutionController.h"
#include "ui/TestPanelBuilder.h"
#include "theme/ThemeManager.h"

#include <QMainWindow>
#include <QFont>
#include <QString>
#include <vector>

class QShortcut;
class QStackedWidget;
class QMenuBar;
class QMenu;
class QsciScintilla;
class ActivityBarButton;
class CollapsibleSplitter;
class LandingButton;

struct TestCase {
    QString input;
    QString expectedOutput;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void onSidePanelCollapsedChanged(bool collapsed);

private:
    void setupUi();
    void setupActivityBar();
    void setupLandingPage();
    void setupMainEditor();
    void setupMenuBar();
    void setupZoomShortcuts();
    void applyUiZoom();

    void addTestCase();
    void removeTestCase(int index);
    void clearAllTestCases();
    void updateTestCaseTitles();
    int indexForButton(const QPushButton *button) const;
    ExecutionController::UiBindings makeBindings(
        const TestPanelBuilder::CaseWidgets &widgets) const;

    // Core managers
    ThemeManager themeManager_;
    TestPanelBuilder testPanelBuilder_;
    EditorConfigurator *editorConfigurator_;
    ExecutionController *executionController_;

    // Main layout
    QStackedWidget *mainStack_ = nullptr;
    CollapsibleSplitter *mainSplitter_ = nullptr;

    // Activity bar
    QWidget *activityBar_ = nullptr;
    ActivityBarButton *sidebarToggle_ = nullptr;
    ActivityBarButton *newFileButton_ = nullptr;
    ActivityBarButton *settingsButton_ = nullptr;
    ActivityBarButton *backButton_ = nullptr;

    // Landing page
    QWidget *landingPage_ = nullptr;
    LandingButton *trainingButton_ = nullptr;
    LandingButton *contestButton_ = nullptr;

    // Editor area
    QWidget *sidePanel_ = nullptr;
    QsciScintilla *codeEditor_ = nullptr;
    TestPanelBuilder::PanelWidgets testPanelWidgets_;
    std::vector<TestPanelBuilder::CaseWidgets> caseWidgets_;

    // Menu
    QMenuBar *menuBar_ = nullptr;
    QMenu *fileMenu_ = nullptr;
    QMenu *editMenu_ = nullptr;

    // Zoom
    double uiScale_ = 1.0;
    QFont baseAppFont_;
    QShortcut *zoomInShortcut_ = nullptr;
    QShortcut *zoomOutShortcut_ = nullptr;
    QShortcut *zoomResetShortcut_ = nullptr;

    // Test case data
    std::vector<TestCase> testCases_;
};
