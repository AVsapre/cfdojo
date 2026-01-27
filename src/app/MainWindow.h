#pragma once

#include "editor/EditorConfigurator.h"
#include "execution/ExecutionController.h"
#include "execution/ParallelExecutor.h"
#include "ui/TestPanelBuilder.h"
#include "theme/ThemeManager.h"

#include <QMainWindow>
#include <QFont>
#include <QString>
#include <QJsonObject>
#include <deque>
#include <vector>

class QShortcut;
class QStackedWidget;
class QMenuBar;
class QMenu;
class QsciScintilla;
class QFileSystemModel;
class QTreeView;
class ActivityBarButton;
class CollapsibleSplitter;
class LandingButton;
class CompanionListener;
class SettingsDialog;
class QPushButton;
class QLabel;
class QTimer;
class QPlainTextEdit;
class QLineEdit;
class QStandardItemModel;
template <typename T>
class QFutureWatcher;

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
    void newFile();
    void openFile();
    void saveFile();
    void saveFileAs();
    void openSettingsDialog();
    void onProblemReceived(const QJsonObject &problem);

private:
    struct StressResult {
        bool passed = false;
        int failedIndex = -1;
        int totalCount = 0;
        QString input;
        QString expected;
        QString actual;
        QString error;
        QString stderrOutput;
        QString complexity;
    };

    enum class EditorMode {
        Solution,
        Brute,
        Generator,
        Template,
        Problem,
        Testcases
    };

    void setupUi();
    void setupActivityBar();
    void setupLandingPage();
    void setupMainEditor();
    void setupMenuBar();
    void setupZoomShortcuts();
    void setupCompanionListener();
    void applyUiZoom();
    void showCopyToast();
    void markDirty();
    void setDirty(bool dirty);
    void runStressTest();
    StressResult runStressTestWorker(int count,
                                     const QString &solution,
                                     const QString &brute,
                                     const QString &generator,
                                     const QString &tmpl,
                                     int timeoutMs,
                                     bool parallel) const;
    QString normalizeText(const QString &text) const;
    void updateTestSummary(const QString &text);
    void applyParallelResult(const TestResult &result);

    void addTestCase();
    void removeTestCase(int index);
    void clearAllTestCases();
    void updateTestCaseTitles();
    void runAllTests();
    void runNextSequentialTest();
    void cancelSequentialRunAll();
    void applyCompileErrorToAllCases(const QString &error);
    int indexForButton(const QPushButton *button) const;
    ExecutionController::UiBindings makeBindings(
        const TestPanelBuilder::CaseWidgets &widgets) const;
    void setEditorMode(EditorMode mode);
    void syncEditorToMode();
    void updateEditorModeButtons();
    void updateWindowTitle();
    QString buildProblemJson() const;
    QString buildTestcasesJson() const;
    void updateActivityBarActiveStates(bool collapsed);

    // Core managers
    ThemeManager themeManager_;
    TestPanelBuilder testPanelBuilder_;
    EditorConfigurator *editorConfigurator_;
    ExecutionController *executionController_;
    ParallelExecutor *parallelExecutor_;

    // Main layout
    QStackedWidget *mainStack_ = nullptr;
    CollapsibleSplitter *mainSplitter_ = nullptr;

    // Activity bar
    QWidget *activityBar_ = nullptr;
    ActivityBarButton *sidebarToggle_ = nullptr;
    ActivityBarButton *stressTestButton_ = nullptr;
    ActivityBarButton *templateButton_ = nullptr;
    ActivityBarButton *newFileButton_ = nullptr;
    ActivityBarButton *settingsButton_ = nullptr;
    ActivityBarButton *backButton_ = nullptr;

    // Landing page
    QWidget *landingPage_ = nullptr;
    LandingButton *trainingButton_ = nullptr;
    LandingButton *contestButton_ = nullptr;

    // Editor area
    QWidget *sidePanel_ = nullptr;
    QStackedWidget *sideStack_ = nullptr;
    QWidget *fileExplorer_ = nullptr;
    QWidget *stressTestPanel_ = nullptr;
    QWidget *cpackPanel_ = nullptr;
    QPushButton *stressRunButton_ = nullptr;
    QLabel *stressStatusLabel_ = nullptr;
    QPlainTextEdit *stressLog_ = nullptr;
    QLineEdit *stressCountEdit_ = nullptr;
    QFutureWatcher<StressResult> *stressWatcher_ = nullptr;
    bool stressRunning_ = false;
    std::vector<double> runAllInputSizes_;
    std::vector<double> runAllTimesMs_;
    bool runAllCollecting_ = false;
    QTreeView *cpackTree_ = nullptr;
    QStandardItemModel *cpackModel_ = nullptr;
    QTreeView *fileTree_ = nullptr;
    QFileSystemModel *fileModel_ = nullptr;
    QsciScintilla *codeEditor_ = nullptr;
    TestPanelBuilder::PanelWidgets testPanelWidgets_;
    std::vector<TestPanelBuilder::CaseWidgets> caseWidgets_;

    // Menu
    QMenuBar *menuBar_ = nullptr;
    QMenu *fileMenu_ = nullptr;
    QMenu *editMenu_ = nullptr;
    QWidget *copyToast_ = nullptr;
    QLabel *copyToastLabel_ = nullptr;
    QTimer *copyToastTimer_ = nullptr;

    // Zoom
    double uiScale_ = 1.0;
    QFont baseAppFont_;
    QShortcut *zoomInShortcut_ = nullptr;
    QShortcut *zoomOutShortcut_ = nullptr;
    QShortcut *zoomResetShortcut_ = nullptr;

    // Test case data
    std::vector<TestCase> testCases_;
    
    // Current file
    QString currentFilePath_;
    EditorMode editorMode_ = EditorMode::Solution;
    QString currentSolutionCode_;
    QString baseWindowTitle_ = "CF Dojo - v1.0";

    // Optional additional sources stored in .cpack
    QString currentBruteCode_;
    QString currentGeneratorCode_;
    QString currentProblemRaw_;
    QString currentTestcasesRaw_;
    bool problemEdited_ = false;
    bool testcasesEdited_ = false;
    
    // Template with //#main transclusion marker (default is just //#main)
    QString currentTemplate_ = "//#main";
    
    // Execution timeout in seconds (per-problem setting)
    int currentTimeout_ = 5;
    
    // Competitive Companion
    CompanionListener *companionListener_ = nullptr;
    QJsonObject currentProblem_;
    
    // Settings window
    SettingsDialog *settingsWindow_ = nullptr;
    
    // Experimental settings
    bool multithreadingEnabled_ = false;

    // Dirty state
    bool isDirty_ = false;
    int dirtySuppressionDepth_ = 0;
    bool hasSavedFile_ = false;

    // Sequential run-all state
    std::deque<int> runAllQueue_;
    bool runAllSequentialActive_ = false;
    int runAllCurrentIndex_ = -1;

    class DirtyScope {
    public:
        explicit DirtyScope(MainWindow *window);
        ~DirtyScope();

    private:
        MainWindow *window_ = nullptr;
    };
};
