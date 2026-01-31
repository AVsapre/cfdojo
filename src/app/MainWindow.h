#pragma once

#include "editor/EditorConfigurator.h"
#include "execution/ExecutionController.h"
#include "execution/ParallelExecutor.h"
#include "ui/TestPanelBuilder.h"
#include "theme/ThemeManager.h"

#include <QMainWindow>
#include <QFont>
#include <QPointer>
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
class CompanionListener;
class SettingsDialog;
class QPushButton;
class QLabel;
class QTimer;
class QPlainTextEdit;
class QLineEdit;
class QStandardItemModel;
class QStandardItem;
class QCloseEvent;
class QShowEvent;
class CpackFileHandler;
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
    void setBaseWindowTitle(const QString &title);
    void startPlainTextSession(const QString &title);

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;

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
    void setupMainEditor();
    void setupMenuBar();
    void openHelpDialog();
    void openAboutDialog();
    void setupZoomShortcuts();
    void setupCompanionListener();
    void applyUiZoom();
    void showCopyToast();
    void markDirty();
    void setDirty(bool dirty);
    void saveFileAsWithTitle(const QString &title);
    void openPlainFileWithPrompt(const QString &path);
    void populateCpackTree();
    void setPlainTextMode(bool enabled);
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
    void updateProblemMetaUi();
    bool confirmDiscardUnsaved(const QString &actionLabel);
    void setupAutosave();
    void scheduleAutosave();
    void performAutosave();
    void clearAutosaveFiles();
    void checkForRecovery();
    QString autosaveDir() const;
    QString autosaveCpackPath() const;
    QString autosaveMetaPath() const;
    QString autosaveSessionPath() const;
    void loadCpackFromHandler(const CpackFileHandler &handler,
                              const QString &path,
                              bool markSavedFile);
    QString buildProblemJson() const;
    QString buildTestcasesJson() const;
    void updateActivityBarActiveStates(bool collapsed);
    void updateTemplateAvailability();
    void updateTemplateMarkerVisibility();
    void syncTemplateToggleUi();

    // Core managers
    ThemeManager themeManager_;
    TestPanelBuilder testPanelBuilder_;
    EditorConfigurator *editorConfigurator_;
    ExecutionController *executionController_;
    ParallelExecutor *parallelExecutor_;

    // Main layout
    CollapsibleSplitter *mainSplitter_ = nullptr;

    // Activity bar
    QWidget *activityBar_ = nullptr;
    ActivityBarButton *sidebarToggle_ = nullptr;
    ActivityBarButton *stressTestButton_ = nullptr;
    ActivityBarButton *templateButton_ = nullptr;
    ActivityBarButton *newFileButton_ = nullptr;
    ActivityBarButton *settingsButton_ = nullptr;
    ActivityBarButton *backButton_ = nullptr;
    QPushButton *menuTemplateButton_ = nullptr;
    QPushButton *menuRunAllButton_ = nullptr;
    QWidget *plainTextBanner_ = nullptr;
    QLabel *plainTextBannerLabel_ = nullptr;
    QPushButton *plainTextConvertButton_ = nullptr;

    // Editor area
    QWidget *sidePanel_ = nullptr;
    QStackedWidget *sideStack_ = nullptr;
    QWidget *fileExplorer_ = nullptr;
    QWidget *stressTestPanel_ = nullptr;
    QWidget *cpackPanel_ = nullptr;
    QPushButton *stressRunButton_ = nullptr;
    QLabel *stressStatusLabel_ = nullptr;
    QLabel *stressComplexityLabel_ = nullptr;
    QPlainTextEdit *stressLog_ = nullptr;
    QLineEdit *stressCountEdit_ = nullptr;
    QFutureWatcher<StressResult> *stressWatcher_ = nullptr;
    bool stressRunning_ = false;
    std::vector<double> runAllInputSizes_;
    std::vector<double> runAllTimesMs_;
    bool runAllCollecting_ = false;
    QTreeView *cpackTree_ = nullptr;
    QStandardItemModel *cpackModel_ = nullptr;
    QStandardItem *cpackTemplateItem_ = nullptr;
    QTreeView *fileTree_ = nullptr;
    QFileSystemModel *fileModel_ = nullptr;
    QsciScintilla *codeEditor_ = nullptr;
    TestPanelBuilder::PanelWidgets testPanelWidgets_;
    std::vector<TestPanelBuilder::CaseWidgets> caseWidgets_;

    // Menu
    QMenuBar *menuBar_ = nullptr;
    QMenu *fileMenu_ = nullptr;
    QMenu *editMenu_ = nullptr;
    QMenu *helpMenu_ = nullptr;
    QWidget *copyToast_ = nullptr;
    QLabel *copyToastLabel_ = nullptr;
    QTimer *copyToastTimer_ = nullptr;
    QTimer *autosaveTimer_ = nullptr;
    bool recoveryChecked_ = false;

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

    // Active problem state
    QString currentBruteCode_;
    QString currentGeneratorCode_;
    QString currentProblemRaw_;
    QString currentTestcasesRaw_;
    bool problemEdited_ = false;
    bool testcasesEdited_ = false;
    QString currentTemplate_ = "//#main";
    int currentTimeout_ = 5;
    
    // Competitive Companion
    CompanionListener *companionListener_ = nullptr;
    QJsonObject currentProblem_;
    
    // Settings window
    QPointer<SettingsDialog> settingsWindow_;
    
    // Experimental settings
    bool multithreadingEnabled_ = false;
    bool transcludeTemplateEnabled_ = false;
    int autosaveIntervalMs_ = 15000;

    // Dirty state
    bool isDirty_ = false;
    int dirtySuppressionDepth_ = 0;
    bool hasSavedFile_ = false;
    bool plainTextMode_ = false;
    bool wasSidePanelCollapsed_ = false;

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
