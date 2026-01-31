#include "app/MainWindow.h"
#include "app/ActivityBarButton.h"
#include "app/CollapsibleSplitter.h"
#include "app/SettingsDialog.h"
#include "companion/CompanionListener.h"
#include "file/CpackFileHandler.h"
#include "ui/AutoResizingTextEdit.h"
#include "ui/FileExplorerBuilder.h"
#include "ui/IconUtils.h"
#include "ui/StressPanelBuilder.h"

#include <Qsci/qsciscintilla.h>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDateTime>
#include <QDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QFileSystemModel>
#include <QTreeView>
#include <QFile>
#include <QIcon>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QScrollArea>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QDir>
#include <QScreen>
#include <QWindow>
#include <QShowEvent>
#include <QStringConverter>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QStringList>
#include <QFutureWatcher>
#include <QVector>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtConcurrent>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kActivityBarWidth = 50;
constexpr int kSidePanelDefaultWidth = 240;
constexpr int kSidePanelMinWidth = 175;
const QString kTemplateMarker = QStringLiteral("//#main");

QString loadDefaultTemplate() {
    QSettings settings("CF Dojo", "CF Dojo");
    const QString stored = settings.value("defaultTemplate").toString();
    return stored.isEmpty() ? kTemplateMarker : stored;
}

struct RegressionResult {
    bool ok = false;
    double slope = 0.0;
    double r2 = 0.0;
    double maxMs = 0.0;
};

RegressionResult computeLogLogRegression(const std::vector<double> &sizes,
                                         const std::vector<double> &timesMs) {
    RegressionResult result;
    std::vector<double> xs;
    std::vector<double> ys;
    xs.reserve(sizes.size());
    ys.reserve(timesMs.size());

    for (size_t i = 0; i < sizes.size() && i < timesMs.size(); ++i) {
        const double n = sizes[i];
        const double t = timesMs[i];
        if (n > 0.0) {
            const double tForFit = t > 0.0 ? t : 1.0;
            xs.push_back(std::log(n));
            ys.push_back(std::log(tForFit));
            if (t > 0.0) {
                result.maxMs = std::max(result.maxMs, t);
            }
        }
    }

    if (xs.size() < 3) {
        return result;
    }

    double sumX = 0.0;
    double sumY = 0.0;
    for (size_t i = 0; i < xs.size(); ++i) {
        sumX += xs[i];
        sumY += ys[i];
    }
    const double meanX = sumX / xs.size();
    const double meanY = sumY / ys.size();

    double num = 0.0;
    double den = 0.0;
    for (size_t i = 0; i < xs.size(); ++i) {
        const double dx = xs[i] - meanX;
        num += dx * (ys[i] - meanY);
        den += dx * dx;
    }

    if (den <= 0.0) {
        return result;
    }

    const double slope = num / den;
    const double intercept = meanY - slope * meanX;

    double ssTot = 0.0;
    double ssRes = 0.0;
    for (size_t i = 0; i < xs.size(); ++i) {
        const double fit = intercept + slope * xs[i];
        ssTot += (ys[i] - meanY) * (ys[i] - meanY);
        ssRes += (ys[i] - fit) * (ys[i] - fit);
    }
    const double r2 = ssTot > 0.0 ? (1.0 - (ssRes / ssTot)) : 0.0;

    result.ok = true;
    result.slope = slope;
    result.r2 = r2;
    return result;
}

QString estimateComplexityLabel(const std::vector<double> &sizes,
                                const std::vector<double> &timesMs) {
    const RegressionResult reg = computeLogLogRegression(sizes, timesMs);
    if (!reg.ok) {
        return QString();
    }
    return QString("Estimated: T ≈ a·n^%1 (R²=%2)")
        .arg(reg.slope, 0, 'f', 2)
        .arg(reg.r2, 0, 'f', 2);
}

QString suspectedComplexityLabel(const std::vector<double> &sizes,
                                 const std::vector<double> &timesMs) {
    const RegressionResult reg = computeLogLogRegression(sizes, timesMs);
    if (!reg.ok) {
        return QString("Suspected: insufficient timing data");
    }
    struct Bucket {
        const char *label;
        const char *title;
        const char *desc;
    };

    const double k = reg.slope;
    Bucket bucket;
    if (k < 0.15) {
        bucket = {"O(1)", "constant", "Doesn’t scale with input."};
    } else if (k < 0.5) {
        bucket = {"O(log n)", "logarithmic", "Grows very slowly. Doubling n adds ~1 step."};
    } else if (k < 1.15) {
        bucket = {"O(n)", "linear", "Double input → double work."};
    } else if (k < 1.6) {
        bucket = {"O(n log n)", "near-linear", "Slightly worse than linear, still excellent."};
    } else if (k < 2.4) {
        bucket = {"O(n²)", "quadratic", "Works for small n, explodes fast."};
    } else if (k < 3.2) {
        bucket = {"O(n³)", "cubic", "Only acceptable for very small n."};
    } else {
        bucket = {"O(2ⁿ)/O(n!)", "exponential / factorial", "Impossible past tiny n."};
    }

    const QString maxTime =
        reg.maxMs > 0.0 ? QString(" • max %1 ms").arg(reg.maxMs, 0, 'f', 0)
                        : QString(" • max <1 ms");
    const QString summary = QString("Suspected: %1 — %2")
        .arg(bucket.label)
        .arg(bucket.title);
    const QString fit = QString("Fit: n^%1 (R²=%2)%3")
        .arg(reg.slope, 0, 'f', 2)
        .arg(reg.r2, 0, 'f', 2)
        .arg(maxTime);
    return QString("%1\n%2\n%3")
        .arg(summary)
        .arg(bucket.desc)
        .arg(fit);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      editorConfigurator_(new EditorConfigurator(this)),
      executionController_(new ExecutionController(this)),
      parallelExecutor_(new ParallelExecutor(this)),
      baseAppFont_(qApp->font()) {
    currentTemplate_ = loadDefaultTemplate();
    QSettings settings("CF Dojo", "CF Dojo");
    transcludeTemplateEnabled_ = settings.value("transcludeTemplate", false).toBool();
    const int autosaveSec = std::clamp(
        settings.value("autosaveIntervalSec", 15).toInt(), 5, 300);
    autosaveIntervalMs_ = autosaveSec * 1000;
    executionController_->setTranscludeTemplateEnabled(transcludeTemplateEnabled_);
    themeManager_.apply(qApp, uiScale_);
    executionController_->setIconTintColor(themeManager_.textColor());
    setupUi();
    setupZoomShortcuts();
    setupCompanionListener();
    applyUiZoom();
    setupAutosave();
    
    // Connect parallel executor signals
    connect(parallelExecutor_, &ParallelExecutor::testFinished,
            this, [this](const TestResult &result) {
        applyParallelResult(result);
    });
    
    connect(parallelExecutor_, &ParallelExecutor::allTestsFinished,
            this, [this](const std::vector<TestResult> &results) {
        for (const auto &result : results) {
            applyParallelResult(result);
        }
        if (!runAllCollecting_) {
            return;
        }
        const QString summary = estimateComplexityLabel(runAllInputSizes_, runAllTimesMs_);
        updateTestSummary(summary);
        runAllCollecting_ = false;
    });

    connect(parallelExecutor_, &ParallelExecutor::compilationFinished,
            this, [this](bool success, const QString &error) {
        if (!success) {
            applyCompileErrorToAllCases(error);
            runAllCollecting_ = false;
            updateTestSummary(QString());
        }
    });

    connect(executionController_, &ExecutionController::executionFinished,
            this, [this](const QString &, const QString &, int) {
        if (runAllSequentialActive_) {
            if (runAllCollecting_ &&
                runAllCurrentIndex_ >= 0 &&
                static_cast<size_t>(runAllCurrentIndex_) < runAllTimesMs_.size()) {
                runAllTimesMs_[static_cast<size_t>(runAllCurrentIndex_)] =
                    static_cast<double>(executionController_->lastExecutionTimeMs());
            }
            runNextSequentialTest();
            if (!runAllSequentialActive_ && runAllCollecting_) {
                const QString summary =
                    estimateComplexityLabel(runAllInputSizes_, runAllTimesMs_);
                updateTestSummary(summary);
                runAllCollecting_ = false;
            }
        }
    });

    connect(executionController_, &ExecutionController::compilationFailed,
            this, [this](const QString &error) {
        if (runAllSequentialActive_) {
            applyCompileErrorToAllCases(error);
            cancelSequentialRunAll();
            runAllCollecting_ = false;
            updateTestSummary(QString());
        }
    });
}

MainWindow::~MainWindow() = default;

void MainWindow::setBaseWindowTitle(const QString &title) {
    baseWindowTitle_ = title;
    updateWindowTitle();
}

void MainWindow::startPlainTextSession(const QString &title) {
    DirtyScope guard(this);
    if (codeEditor_) {
        codeEditor_->clear();
    }
    currentFilePath_.clear();
    hasSavedFile_ = false;
    editorMode_ = EditorMode::Solution;
    currentSolutionCode_.clear();
    currentBruteCode_.clear();
    currentGeneratorCode_.clear();
    currentTemplate_ = loadDefaultTemplate();
    currentProblem_ = QJsonObject();
    currentProblemRaw_.clear();
    currentTestcasesRaw_.clear();
    problemEdited_ = false;
    testcasesEdited_ = false;
    currentTimeout_ = 5;
    clearAllTestCases();
    addTestCase();
    setDirty(false);
    setBaseWindowTitle(title.isEmpty() ? QString("CF Dojo - Training") : title);
    setPlainTextMode(true);
    updateProblemMetaUi();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!confirmDiscardUnsaved("quitting")) {
        event->ignore();
        return;
    }
    if (autosaveTimer_) {
        autosaveTimer_->stop();
    }
    clearAutosaveFiles();
    QFile::remove(autosaveSessionPath());
    QMainWindow::closeEvent(event);
}

void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event);
    if (recoveryChecked_) {
        return;
    }
    recoveryChecked_ = true;
    QTimer::singleShot(0, this, &MainWindow::checkForRecovery);
}

MainWindow::DirtyScope::DirtyScope(MainWindow *window)
    : window_(window) {
    if (window_) {
        ++window_->dirtySuppressionDepth_;
    }
}

MainWindow::DirtyScope::~DirtyScope() {
    if (window_ && window_->dirtySuppressionDepth_ > 0) {
        --window_->dirtySuppressionDepth_;
    }
}

void MainWindow::setupUi() {
    resize(1200, 800);
    baseWindowTitle_ = "CF Dojo - v1.0";
    updateWindowTitle();

    setupMenuBar();
    setupMainEditor();
    setCentralWidget(mainSplitter_);
}

void MainWindow::setupActivityBar() {
    activityBar_ = new QWidget();
    activityBar_->setObjectName("ActivityBar");
    activityBar_->setFixedWidth(kActivityBarWidth);
    activityBar_->setAttribute(Qt::WA_StyledBackground, true);

    auto applyActivityTint = [this](ActivityBarButton *button) {
        if (!button) {
            return;
        }
        button->setTintColors(themeManager_.textColor(),
                              themeManager_.textColor(),
                              QColor("#808080"));
    };

    auto *barLayout = new QHBoxLayout(activityBar_);
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setSpacing(0);

    auto *buttonColumn = new QWidget(activityBar_);
    buttonColumn->setObjectName("ActivityBarButtons");
    auto *layout = new QVBoxLayout(buttonColumn);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Sidebar toggle at top
    sidebarToggle_ = new ActivityBarButton(":/images/testcase.svg", activityBar_);
    sidebarToggle_->setObjectName("SidebarToggle");
    sidebarToggle_->setFixedHeight(kActivityBarWidth);
    sidebarToggle_->setToolTip("Toggle side bar");
    sidebarToggle_->setCheckable(true);
    sidebarToggle_->setChecked(true);
    applyActivityTint(sidebarToggle_);

    connect(sidebarToggle_, &QPushButton::clicked, this, [this]() {
        if (!mainSplitter_ || !sideStack_) {
            return;
        }
        if (mainSplitter_->isCollapsed()) {
            sideStack_->setCurrentWidget(testPanelWidgets_.panel);
            mainSplitter_->expand();
            sidebarToggle_->setChecked(true);
            newFileButton_->setChecked(false);
            if (stressTestButton_) {
                stressTestButton_->setChecked(false);
            }
            if (templateButton_) {
                templateButton_->setChecked(false);
            }
            return;
        }
        if (sideStack_->currentWidget() != testPanelWidgets_.panel) {
            sideStack_->setCurrentWidget(testPanelWidgets_.panel);
            sidebarToggle_->setChecked(true);
            newFileButton_->setChecked(false);
            if (stressTestButton_) {
                stressTestButton_->setChecked(false);
            }
            if (templateButton_) {
                templateButton_->setChecked(false);
            }
            return;
        }
        mainSplitter_->collapse();
        sidebarToggle_->setChecked(false);
    });

    // Stress test button
    stressTestButton_ = new ActivityBarButton(":/images/stresstest.svg", activityBar_);
    stressTestButton_->setObjectName("StressTestButton");
    stressTestButton_->setFixedHeight(kActivityBarWidth);
    stressTestButton_->setToolTip("Stress Test");
    stressTestButton_->setCheckable(true);
    stressTestButton_->setChecked(false);
    applyActivityTint(stressTestButton_);

    connect(stressTestButton_, &QPushButton::clicked, this, [this]() {
        if (!mainSplitter_ || !sideStack_ || !stressTestPanel_) {
            return;
        }

        sideStack_->setCurrentWidget(stressTestPanel_);
        if (mainSplitter_->isCollapsed()) {
            mainSplitter_->expand();
        }
        if (stressTestButton_) {
            stressTestButton_->setChecked(true);
        }
        if (sidebarToggle_) {
            sidebarToggle_->setChecked(false);
        }
        if (templateButton_) {
            templateButton_->setChecked(false);
        }
        if (newFileButton_) {
            newFileButton_->setChecked(false);
        }
    });

    // Template editor button
    templateButton_ = new ActivityBarButton(":/images/template.svg", activityBar_);
    templateButton_->setObjectName("TemplateButton");
    templateButton_->setFixedHeight(kActivityBarWidth);
    templateButton_->setToolTip("Show template view");
    templateButton_->setCheckable(true);
    templateButton_->setChecked(false);
    applyActivityTint(templateButton_);

    connect(templateButton_, &QPushButton::clicked, this, [this]() {
        if (!mainSplitter_ || !sideStack_ || !cpackPanel_) {
            return;
        }
        sideStack_->setCurrentWidget(cpackPanel_);
        if (mainSplitter_->isCollapsed()) {
            mainSplitter_->expand();
        }
        templateButton_->setChecked(true);
        if (sidebarToggle_) {
            sidebarToggle_->setChecked(false);
        }
        if (stressTestButton_) {
            stressTestButton_->setChecked(false);
        }
        if (newFileButton_) {
            newFileButton_->setChecked(false);
        }
    });

    layout->addWidget(sidebarToggle_, 0, Qt::AlignTop);
    layout->addWidget(stressTestButton_, 0, Qt::AlignTop);
    layout->addWidget(templateButton_, 0, Qt::AlignTop);
    layout->addStretch();

    // Bottom buttons container
    auto *bottomPanel = new QWidget(buttonColumn);
    auto *bottomLayout = new QVBoxLayout(bottomPanel);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(0);

    // New file button (bottom section)
    newFileButton_ = new ActivityBarButton(":/images/file.svg", bottomPanel);
    newFileButton_->setObjectName("NewFileButton");
    newFileButton_->setFixedHeight(kActivityBarWidth);
    newFileButton_->setToolTip("File Explorer");
    newFileButton_->setCheckable(true);
    newFileButton_->setChecked(false);
    applyActivityTint(newFileButton_);

    connect(newFileButton_, &QPushButton::clicked, this, [this]() {
        if (!mainSplitter_ || !sideStack_ || !fileExplorer_) {
            return;
        }
        sideStack_->setCurrentWidget(fileExplorer_);
        if (mainSplitter_->isCollapsed()) {
            mainSplitter_->expand();
        }
        newFileButton_->setChecked(true);
        sidebarToggle_->setChecked(false);
        if (stressTestButton_) {
            stressTestButton_->setChecked(false);
        }
        if (templateButton_) {
            templateButton_->setChecked(false);
        }
    });

    settingsButton_ = new ActivityBarButton(":/images/settings.svg", bottomPanel);
    settingsButton_->setObjectName("SettingsButton");
    settingsButton_->setFixedHeight(kActivityBarWidth);
    settingsButton_->setToolTip("Settings");
    settingsButton_->setCheckable(true);
    applyActivityTint(settingsButton_);

    connect(settingsButton_, &QPushButton::clicked, this, &MainWindow::openSettingsDialog);

    backButton_ = new ActivityBarButton(":/images/arrow-left.svg", bottomPanel);
    backButton_->setObjectName("BackButton");
    backButton_->setFixedHeight(kActivityBarWidth);
    backButton_->setToolTip("Back");
    applyActivityTint(backButton_);

    connect(backButton_, &QPushButton::clicked, this, [this]() {
        close();
    });

    bottomLayout->addWidget(newFileButton_);
    bottomLayout->addWidget(settingsButton_);
    bottomLayout->addWidget(backButton_);
    layout->addWidget(bottomPanel, 0, Qt::AlignBottom);

    barLayout->addWidget(buttonColumn);

    auto *edgeLine = new QWidget(activityBar_);
    edgeLine->setObjectName("ActivityBarEdge");
    edgeLine->setFixedWidth(1);
    edgeLine->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    barLayout->addWidget(edgeLine);
    syncTemplateToggleUi();
}

void MainWindow::openSettingsDialog() {
    if (!settingsWindow_) {
        settingsWindow_ = new SettingsDialog(this);
        settingsWindow_->setTemplate(currentTemplate_);
        settingsWindow_->setMultithreadingEnabled(multithreadingEnabled_);
        settingsWindow_->setTranscludeTemplateEnabled(transcludeTemplateEnabled_);
        {
            QSettings settings("CF Dojo", "CF Dojo");
            settingsWindow_->setAutosaveIntervalSeconds(
                settings.value("autosaveIntervalSec", 15).toInt());
            settingsWindow_->setDefaultLanguage(
                settings.value("defaultLanguage", "C++").toString());
            settingsWindow_->setCompilerPath(
                settings.value("cppCompilerPath", "g++").toString());
            settingsWindow_->setCompilerFlags(
                settings.value("cppCompilerFlags", "-O2 -std=c++17").toString());
            settingsWindow_->setPythonPath(
                settings.value("pythonPath", "python3").toString());
            settingsWindow_->setPythonArgs(
                settings.value("pythonArgs", "").toString());
            settingsWindow_->setJavaCompilerPath(
                settings.value("javaCompilerPath", "javac").toString());
            settingsWindow_->setJavaRunPath(
                settings.value("javaRunPath", "java").toString());
            settingsWindow_->setJavaArgs(
                settings.value("javaArgs", "").toString());
        }

        connect(settingsWindow_, &SettingsDialog::settingsChanged, this, [this]() {
            currentTemplate_ = settingsWindow_->getTemplate();
            markDirty();
        });

        connect(settingsWindow_, &QWidget::destroyed, this, [this]() {
            if (settingsButton_) {
                settingsButton_->setChecked(false);
                settingsButton_->setActiveState(false);
            }
        });

        connect(settingsWindow_, &SettingsDialog::closed, this, [this]() {
            if (settingsButton_) {
                settingsButton_->setChecked(false);
                settingsButton_->setActiveState(false);
            }
        });

        connect(settingsWindow_, &SettingsDialog::saved, this, [this]() {
            currentTemplate_ = settingsWindow_->getTemplate();
            QSettings settings("CF Dojo", "CF Dojo");
            settings.setValue("defaultTemplate", currentTemplate_);
            multithreadingEnabled_ = settingsWindow_->isMultithreadingEnabled();
            transcludeTemplateEnabled_ = settingsWindow_->isTranscludeTemplateEnabled();
            settings.setValue("transcludeTemplate", transcludeTemplateEnabled_);
            executionController_->setTranscludeTemplateEnabled(transcludeTemplateEnabled_);
            updateTemplateAvailability();
            settings.setValue("autosaveIntervalSec", settingsWindow_->autosaveIntervalSeconds());
            autosaveIntervalMs_ = settingsWindow_->autosaveIntervalSeconds() * 1000;
            if (isDirty_) {
                scheduleAutosave();
            }
            settings.setValue("defaultLanguage", settingsWindow_->defaultLanguage());
            settings.setValue("cppCompilerPath", settingsWindow_->compilerPath());
            settings.setValue("cppCompilerFlags", settingsWindow_->compilerFlags());
            settings.setValue("pythonPath", settingsWindow_->pythonPath());
            settings.setValue("pythonArgs", settingsWindow_->pythonArgs());
            settings.setValue("javaCompilerPath", settingsWindow_->javaCompilerPath());
            settings.setValue("javaRunPath", settingsWindow_->javaRunPath());
            settings.setValue("javaArgs", settingsWindow_->javaArgs());
            if (settingsButton_) {
                settingsButton_->setChecked(false);
                settingsButton_->setActiveState(false);
            }
        });

        connect(settingsWindow_, &SettingsDialog::cancelled, this, [this]() {
            if (settingsButton_) {
                settingsButton_->setChecked(false);
                settingsButton_->setActiveState(false);
            }
        });
    } else {
        settingsWindow_->setTemplate(currentTemplate_);
        settingsWindow_->setMultithreadingEnabled(multithreadingEnabled_);
        settingsWindow_->setTranscludeTemplateEnabled(transcludeTemplateEnabled_);
        QSettings settings("CF Dojo", "CF Dojo");
        settingsWindow_->setAutosaveIntervalSeconds(
            settings.value("autosaveIntervalSec", 15).toInt());
        settingsWindow_->setDefaultLanguage(
            settings.value("defaultLanguage", "C++").toString());
        settingsWindow_->setCompilerPath(
            settings.value("cppCompilerPath", "g++").toString());
        settingsWindow_->setCompilerFlags(
            settings.value("cppCompilerFlags", "-O2 -std=c++17").toString());
        settingsWindow_->setPythonPath(
            settings.value("pythonPath", "python3").toString());
        settingsWindow_->setPythonArgs(
            settings.value("pythonArgs", "").toString());
        settingsWindow_->setJavaCompilerPath(
            settings.value("javaCompilerPath", "javac").toString());
        settingsWindow_->setJavaRunPath(
            settings.value("javaRunPath", "java").toString());
        settingsWindow_->setJavaArgs(
            settings.value("javaArgs", "").toString());
    }

    if (settingsButton_) {
        settingsButton_->setChecked(true);
        settingsButton_->setActiveState(true);
    }
    settingsWindow_->show();
    settingsWindow_->raise();
    settingsWindow_->activateWindow();
}

void MainWindow::setupMainEditor() {
    // Build editor and side panel
    const auto editorWidgets = editorConfigurator_->build(this, themeManager_);
    codeEditor_ = editorWidgets.editor;
    if (codeEditor_) {
        connect(codeEditor_, &QsciScintilla::textChanged, this, [this]() {
            markDirty();
            if (transcludeTemplateEnabled_ && editorMode_ == EditorMode::Template) {
                updateTemplateMarkerVisibility();
            }
        });
        codeEditor_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(codeEditor_, &QsciScintilla::customContextMenuRequested, this,
                [this](const QPoint &pos) {
            if (!codeEditor_) {
                return;
            }
            QMenu menu(codeEditor_);

            QAction *undoAction = menu.addAction("Undo");
            undoAction->setEnabled(codeEditor_->isUndoAvailable());
            connect(undoAction, &QAction::triggered, codeEditor_, &QsciScintilla::undo);

            QAction *redoAction = menu.addAction("Redo");
            redoAction->setEnabled(codeEditor_->isRedoAvailable());
            connect(redoAction, &QAction::triggered, codeEditor_, &QsciScintilla::redo);

            menu.addSeparator();

            const bool hasSelection = codeEditor_->hasSelectedText();
            QAction *cutAction = menu.addAction("Cut");
            cutAction->setEnabled(hasSelection);
            connect(cutAction, &QAction::triggered, codeEditor_, &QsciScintilla::cut);

            QAction *copyAction = menu.addAction("Copy");
            copyAction->setEnabled(hasSelection);
            connect(copyAction, &QAction::triggered, codeEditor_, &QsciScintilla::copy);

            QAction *pasteAction = menu.addAction("Paste");
            const QClipboard *clipboard = QApplication::clipboard();
            pasteAction->setEnabled(clipboard && !clipboard->text().isEmpty());
            connect(pasteAction, &QAction::triggered, codeEditor_, &QsciScintilla::paste);

            menu.addSeparator();

            QAction *selectAllAction = menu.addAction("Select All");
            connect(selectAllAction, &QAction::triggered, codeEditor_, &QsciScintilla::selectAll);

            menu.exec(codeEditor_->mapToGlobal(pos));
        });
    }

    testPanelWidgets_ = testPanelBuilder_.build(this, this, themeManager_.textColor());
    updateProblemMetaUi();

    sideStack_ = new QStackedWidget();
    sideStack_->setObjectName("SidePanelStack");
    sideStack_->addWidget(testPanelWidgets_.panel);

    {
        FileExplorerBuilder explorerBuilder;
        const QString rootPath = QDir::currentPath();
        const auto explorerWidgets = explorerBuilder.build(sideStack_, rootPath);
        fileExplorer_ = explorerWidgets.panel;
        fileTree_ = explorerWidgets.tree;
        fileModel_ = explorerWidgets.model;
        sideStack_->addWidget(fileExplorer_);
    }

    // CPack file editor panel
    {
        cpackPanel_ = new QWidget(sideStack_);
        cpackPanel_->setObjectName("CpackPanel");
        auto *cpackLayout = new QVBoxLayout(cpackPanel_);
        cpackLayout->setContentsMargins(12, 12, 12, 12);
        cpackLayout->setSpacing(8);

        auto *cpackTitle = new QLabel("CPack Files", cpackPanel_);
        cpackTitle->setObjectName("PanelTitle");
        cpackLayout->addWidget(cpackTitle);

        cpackModel_ = new QStandardItemModel(cpackPanel_);
        cpackTree_ = new QTreeView(cpackPanel_);
        cpackTree_->setObjectName("CpackTree");
        cpackTree_->setModel(cpackModel_);
        cpackTree_->setHeaderHidden(true);
        cpackTree_->setIndentation(12);
        cpackTree_->setRootIsDecorated(false);
        cpackTree_->setItemsExpandable(false);
        cpackTree_->setEditTriggers(QAbstractItemView::NoEditTriggers);
        cpackTree_->setSelectionBehavior(QAbstractItemView::SelectRows);
        cpackTree_->setSelectionMode(QAbstractItemView::SingleSelection);
        cpackTree_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        const QIcon fileIcon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
        auto addCpackItem = [this, &fileIcon](const QString &label, EditorMode mode) {
            auto *item = new QStandardItem(fileIcon, label);
            item->setData(static_cast<int>(mode), Qt::UserRole);
            item->setEditable(false);
            cpackModel_->appendRow(item);
            return item;
        };

        addCpackItem("solution.cpp", EditorMode::Solution);
        addCpackItem("brute.cpp", EditorMode::Brute);
        addCpackItem("generator.cpp", EditorMode::Generator);
        cpackTemplateItem_ = addCpackItem("template.cpp", EditorMode::Template);
        addCpackItem("problem.json", EditorMode::Problem);
        addCpackItem("testcases.json", EditorMode::Testcases);

        if (cpackTree_->selectionModel()) {
            connect(cpackTree_->selectionModel(), &QItemSelectionModel::currentChanged,
                    this, [this](const QModelIndex &current) {
                if (!current.isValid()) {
                    return;
                }
                const QVariant data = current.data(Qt::UserRole);
                if (!data.isValid()) {
                    return;
                }
                const auto mode = static_cast<EditorMode>(data.toInt());
                if (mode == EditorMode::Template && !transcludeTemplateEnabled_) {
                    return;
                }
                setEditorMode(mode);
            });
        }

        cpackLayout->addWidget(cpackTree_);
        sideStack_->addWidget(cpackPanel_);

        updateProblemMetaUi();
    }

    connect(fileTree_, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        if (!fileModel_ || !codeEditor_ || fileModel_->isDir(index)) {
            return;
        }
        if (!confirmDiscardUnsaved("opening another file")) {
            return;
        }
        const QString filePath = fileModel_->filePath(index);
        if (filePath.endsWith(".cpack", Qt::CaseInsensitive)) {
            CpackFileHandler handler;
            if (!handler.load(filePath)) {
                QMessageBox::critical(this, "Error",
                    "Failed to open file: " + handler.errorString());
                return;
            }
            loadCpackFromHandler(handler, filePath, true);
            return;
        }
        openPlainFileWithPrompt(filePath);
    });

    {
        StressPanelBuilder stressBuilder;
        const auto stressWidgets = stressBuilder.build(sideStack_, themeManager_.textColor());
        stressTestPanel_ = stressWidgets.panel;
        stressStatusLabel_ = stressWidgets.statusLabel;
        stressComplexityLabel_ = stressWidgets.complexityLabel;
        stressRunButton_ = stressWidgets.runButton;
        stressCountEdit_ = stressWidgets.countEdit;
        stressLog_ = stressWidgets.log;
    }

    sideStack_->addWidget(stressTestPanel_);
    sideStack_->setCurrentWidget(testPanelWidgets_.panel);

    populateCpackTree();
    updateTemplateAvailability();
    updateEditorModeButtons();

    if (stressRunButton_) {
        connect(stressRunButton_, &QPushButton::clicked,
                this, &MainWindow::runStressTest);
    }

    sidePanel_ = sideStack_;
    sidePanel_->setMinimumHeight(0);
    sidePanel_->setMinimumWidth(kSidePanelMinWidth);
    sidePanel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Connect test panel buttons
    if (testPanelWidgets_.runAllButton) {
        connect(testPanelWidgets_.runAllButton, &QPushButton::clicked, this,
                &MainWindow::runAllTests);
    }
    if (testPanelWidgets_.addButton) {
        connect(testPanelWidgets_.addButton, &QPushButton::clicked, this, 
                &MainWindow::addTestCase);
    }
    if (testPanelWidgets_.clearCasesButton) {
        connect(testPanelWidgets_.clearCasesButton, &QPushButton::clicked, this,
                &MainWindow::clearAllTestCases);
    }

    // Create initial test case
    {
        DirtyScope guard(this);
        addTestCase();
    }

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
    auto *editorWrapper = new QWidget(this);
    auto *editorLayout = new QVBoxLayout(editorWrapper);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    plainTextBanner_ = new QWidget(editorWrapper);
    plainTextBanner_->setObjectName("PlainTextBanner");
    auto *bannerLayout = new QHBoxLayout(plainTextBanner_);
    bannerLayout->setContentsMargins(12, 8, 12, 8);
    bannerLayout->setSpacing(8);
    plainTextBannerLabel_ = new QLabel(
        "This is a plain text file. Convert to .cpack to enable templates and tests.",
        plainTextBanner_);
    plainTextBannerLabel_->setWordWrap(true);
    plainTextConvertButton_ = new QPushButton("Convert to CPack", plainTextBanner_);
    bannerLayout->addWidget(plainTextBannerLabel_, 1);
    bannerLayout->addWidget(plainTextConvertButton_);
    plainTextBanner_->setVisible(false);

    connect(plainTextConvertButton_, &QPushButton::clicked, this, [this]() {
        saveFileAsWithTitle("Save as CPack");
        if (hasSavedFile_) {
            setPlainTextMode(false);
        }
    });

    editorLayout->addWidget(plainTextBanner_);
    editorLayout->addWidget(editorWidgets.container, 1);
    mainSplitter_->addWidget(editorWrapper);

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

    if (sideStack_) {
        connect(sideStack_, &QStackedWidget::currentChanged, this, [this](int) {
            const bool collapsed = mainSplitter_ && mainSplitter_->isCollapsed();
            updateActivityBarActiveStates(collapsed);
        });
    }

    updateActivityBarActiveStates(mainSplitter_ && mainSplitter_->isCollapsed());
}

void MainWindow::setupMenuBar() {
    menuBar_ = new QMenuBar(this);
    menuBar_->setObjectName("MainMenuBar");

    fileMenu_ = menuBar_->addMenu("File");
    QAction *newAction = fileMenu_->addAction("New");
    QAction *openAction = fileMenu_->addAction("Open...");
    QAction *saveAction = fileMenu_->addAction("Save");
    QAction *saveAsAction = fileMenu_->addAction("Save As...");
    fileMenu_->addSeparator();
    QAction *exitAction = fileMenu_->addAction("Exit");
    
    connect(newAction, &QAction::triggered, this, &MainWindow::newFile);
    connect(openAction, &QAction::triggered, this, &MainWindow::openFile);
    connect(saveAction, &QAction::triggered, this, &MainWindow::saveFile);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::saveFileAs);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    editMenu_ = menuBar_->addMenu("Edit");
    QAction *undoAction = editMenu_->addAction("Undo");
    QAction *redoAction = editMenu_->addAction("Redo");
    editMenu_->addSeparator();
    QAction *cutAction = editMenu_->addAction("Cut");
    QAction *copyAction = editMenu_->addAction("Copy");
    QAction *pasteAction = editMenu_->addAction("Paste");
    editMenu_->addSeparator();
    QAction *preferencesAction = editMenu_->addAction("Preferences...");

    helpMenu_ = menuBar_->addMenu("Help");
    QAction *helpAction = helpMenu_->addAction("Help");
    QAction *aboutAction = helpMenu_->addAction("About");

    newAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_N));
    openAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    saveAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_S));
    saveAsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));

    undoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Z));
    redoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y));
    cutAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
    copyAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_C));
    pasteAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_V));
    preferencesAction->setShortcut(QKeySequence::Preferences);

    auto invokeOnFocus = [this](const char *slot) {
        QWidget *target = QApplication::focusWidget();
        if (!target && codeEditor_) {
            target = codeEditor_;
        }
        if (target) {
            QMetaObject::invokeMethod(target, slot, Qt::DirectConnection);
        }
    };

    connect(undoAction, &QAction::triggered, this, [invokeOnFocus]() {
        invokeOnFocus("undo");
    });
    connect(redoAction, &QAction::triggered, this, [invokeOnFocus]() {
        invokeOnFocus("redo");
    });
    connect(cutAction, &QAction::triggered, this, [invokeOnFocus]() {
        invokeOnFocus("cut");
    });
    connect(copyAction, &QAction::triggered, this, [invokeOnFocus]() {
        invokeOnFocus("copy");
    });
    connect(pasteAction, &QAction::triggered, this, [invokeOnFocus]() {
        invokeOnFocus("paste");
    });
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::openSettingsDialog);
    connect(helpAction, &QAction::triggered, this, &MainWindow::openHelpDialog);
    connect(aboutAction, &QAction::triggered, this, &MainWindow::openAboutDialog);

    auto *menuCorner = new QWidget(menuBar_);
    menuCorner->setObjectName("MenuBarCorner");
    auto *cornerLayout = new QHBoxLayout(menuCorner);
    cornerLayout->setContentsMargins(0, 0, 8, 0);
    cornerLayout->setSpacing(6);

    auto *runAllButton = new QPushButton(menuCorner);
    runAllButton->setObjectName("MenuRunAllButton");
    runAllButton->setToolTip("Run all test cases");
    runAllButton->setIcon(
        IconUtils::makeTintedIcon(":/images/play.svg",
                                  themeManager_.textColor(),
                                  QSize(16, 16)));
    runAllButton->setIconSize(QSize(16, 16));
    runAllButton->setFixedSize(28, 24);
    runAllButton->setFocusPolicy(Qt::NoFocus);
    cornerLayout->addWidget(runAllButton);
    menuRunAllButton_ = runAllButton;

    auto *templateButton = new QPushButton(menuCorner);
    templateButton->setObjectName("MenuTemplateButton");
    templateButton->setToolTip("Toggle template view");
    templateButton->setIcon(
        IconUtils::makeTintedIcon(":/images/template.svg",
                                  themeManager_.textColor(),
                                  QSize(16, 16)));
    templateButton->setIconSize(QSize(16, 16));
    templateButton->setFixedSize(28, 24);
    templateButton->setFocusPolicy(Qt::NoFocus);
    templateButton->setCheckable(true);
    cornerLayout->addWidget(templateButton);
    menuTemplateButton_ = templateButton;
    syncTemplateToggleUi();

    auto *copySolutionButton = new QPushButton(menuCorner);
    copySolutionButton->setObjectName("MenuCopyButton");
    copySolutionButton->setToolTip("Copy solution with template");
    copySolutionButton->setIcon(
        IconUtils::makeTintedIcon(":/images/copy.svg",
                                  themeManager_.textColor(),
                                  QSize(16, 16)));
    copySolutionButton->setIconSize(QSize(16, 16));
    copySolutionButton->setFixedSize(28, 24);
    copySolutionButton->setFocusPolicy(Qt::NoFocus);
    cornerLayout->addWidget(copySolutionButton);

    auto runAllFromMenu = [this]() {
        if (plainTextMode_) {
            return;
        }
        if (transcludeTemplateEnabled_ && !currentTemplate_.contains("//#main")) {
            QMessageBox::warning(this, "Template Missing",
                                 "The template is missing the //#main marker.\n"
                                 "Add the marker or disable template transclusion.");
            return;
        }
        if (!mainSplitter_ || !sideStack_) {
            return;
        }
        sideStack_->setCurrentWidget(testPanelWidgets_.panel);
        if (mainSplitter_->isCollapsed()) {
            mainSplitter_->expand();
        }
        if (sidebarToggle_) {
            sidebarToggle_->setChecked(true);
        }
        if (stressTestButton_) {
            stressTestButton_->setChecked(false);
        }
        if (templateButton_) {
            templateButton_->setChecked(false);
        }
        if (newFileButton_) {
            newFileButton_->setChecked(false);
        }
        runAllTests();
    };
    connect(runAllButton, &QPushButton::clicked, this, runAllFromMenu);
    auto toggleTemplateView = [this]() {
        if (plainTextMode_) {
            return;
        }
        transcludeTemplateEnabled_ = !transcludeTemplateEnabled_;
        QSettings settings("CF Dojo", "CF Dojo");
        settings.setValue("transcludeTemplate", transcludeTemplateEnabled_);
        executionController_->setTranscludeTemplateEnabled(transcludeTemplateEnabled_);
        updateTemplateAvailability();
        if (settingsWindow_) {
            settingsWindow_->setTranscludeTemplateEnabled(transcludeTemplateEnabled_);
        }
        if (transcludeTemplateEnabled_) {
            setEditorMode(EditorMode::Template);
        } else {
            setEditorMode(EditorMode::Solution);
        }
    };

    connect(templateButton, &QPushButton::clicked, this, toggleTemplateView);

    auto *runAllShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_R), this);
    runAllShortcut->setContext(Qt::ApplicationShortcut);
    connect(runAllShortcut, &QShortcut::activated, this, runAllFromMenu);

    auto copySolutionWithTemplate = [this]() {
        syncEditorToMode();
        const QString solution = currentSolutionCode_;
        QString tmpl = currentTemplate_;
        if (tmpl.isEmpty()) {
            tmpl = "//#main";
        }
        if (transcludeTemplateEnabled_ && !tmpl.contains("//#main")) {
            QMessageBox::warning(this, "Template Missing",
                                 "The template is missing the //#main marker.\n"
                                 "Add the marker or disable template transclusion.");
            return;
        }
        QString result = solution;
        if (tmpl.contains("//#main")) {
            result = tmpl;
            result.replace("//#main", solution);
        }
        if (QClipboard *clipboard = QApplication::clipboard()) {
            clipboard->setText(result);
        }
        showCopyToast();
    };
    connect(copySolutionButton, &QPushButton::clicked, this, copySolutionWithTemplate);

    auto *copyShortcut =
        new QShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C), this);
    copyShortcut->setContext(Qt::ApplicationShortcut);
    connect(copyShortcut, &QShortcut::activated, this, copySolutionWithTemplate);

    menuBar_->setCornerWidget(menuCorner, Qt::TopRightCorner);

    setMenuBar(menuBar_);
}

void MainWindow::openHelpDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Help");
    dialog.setMinimumSize(520, 420);

    ThemeManager theme;
    const ThemeColors &colors = theme.colors();
    const QString panelBg = colors.background.name();
    const QString panelEdge = colors.edge.name();

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *contentPanel = new QWidget(&dialog);
    contentPanel->setObjectName("HelpPanel");
    contentPanel->setAttribute(Qt::WA_StyledBackground, true);
    contentPanel->setStyleSheet(
        QString("QWidget#HelpPanel { background: %1; }").arg(panelBg));

    auto *contentLayout = new QVBoxLayout(contentPanel);
    contentLayout->setContentsMargins(12, 12, 12, 12);
    contentLayout->setSpacing(12);

    auto *scrollArea = new QScrollArea(contentPanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        QString("QScrollArea { background: %1; } QScrollArea > QWidget > QWidget { background: %1; }")
            .arg(panelBg));

    auto *scrollContent = new QWidget(scrollArea);
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(12, 12, 12, 12);
    scrollLayout->setSpacing(12);

    auto *helpText = new QLabel(scrollContent);
    helpText->setTextFormat(Qt::RichText);
    helpText->setWordWrap(true);
    helpText->setText(
        "<h2>Help</h2>"
        "<p>CF Dojo is a local competitive programming IDE for solving and testing problems.</p>"
        "<h3>Quick start</h3>"
        "<ol>"
        "<li>Open a .cpack or import from Competitive Companion.</li>"
        "<li>Write your solution in solution.cpp.</li>"
        "<li>Add test cases and click Run All.</li>"
        "</ol>"
        "<h3>New / Open / Save</h3>"
        "<ul>"
        "<li>New creates a blank problem and prompts for a .cpack file.</li>"
        "<li>Open supports .cpack and plain text files.</li>"
        "<li>Plain text mode has limited features; convert via the banner.</li>"
        "</ul>"
        "<h3>Testing & stress</h3>"
        "<ul>"
        "<li>Run All executes your local test cases.</li>"
        "<li>Stress testing uses generator.cpp + brute.cpp.</li>"
        "</ul>"
        "<h3>Templates</h3>"
        "<p>template.cpp uses //#main to insert your solution.</p>"
        "<h3>Autosave & recovery</h3>"
        "<p>Unsaved changes are autosaved and can be restored after a crash.</p>"
        "<h3>Documentation</h3>"
        "<p>Full guide: docs/quickstart.md</p>");
    scrollLayout->addWidget(helpText);
    scrollLayout->addStretch();

    scrollContent->setLayout(scrollLayout);
    scrollArea->setWidget(scrollContent);
    scrollArea->setStyleSheet(
        QString("QScrollArea { background: %1; border: 1px solid %2; }"
                "QScrollArea > QWidget > QWidget { background: %1; }")
            .arg(panelBg, panelEdge));
    contentLayout->addWidget(scrollArea, 1);

    auto *buttonRow = new QWidget(contentPanel);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(8);
    buttonLayout->addStretch();
    auto *closeBtn = new QPushButton("Close", buttonRow);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);
    contentLayout->addWidget(buttonRow);

    layout->addWidget(contentPanel, 1);

    dialog.exec();
}

void MainWindow::openAboutDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("About");
    dialog.setMinimumSize(520, 460);

    ThemeManager theme;
    const ThemeColors &colors = theme.colors();
    const QString panelBg = colors.background.name();
    const QString panelEdge = colors.edge.name();

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *contentPanel = new QWidget(&dialog);
    contentPanel->setObjectName("HelpPanel");
    contentPanel->setAttribute(Qt::WA_StyledBackground, true);
    contentPanel->setStyleSheet(
        QString("QWidget#HelpPanel { background: %1; }").arg(panelBg));

    auto *contentLayout = new QVBoxLayout(contentPanel);
    contentLayout->setContentsMargins(12, 12, 12, 12);
    contentLayout->setSpacing(12);

    auto *scrollArea = new QScrollArea(contentPanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet(
        QString("QScrollArea { background: %1; } QScrollArea > QWidget > QWidget { background: %1; }")
            .arg(panelBg));

    auto *scrollContent = new QWidget(scrollArea);
    auto *scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(12, 12, 12, 12);
    scrollLayout->setSpacing(12);

    auto *aboutText = new QLabel(scrollContent);
    aboutText->setTextFormat(Qt::RichText);
    aboutText->setOpenExternalLinks(true);
    aboutText->setWordWrap(true);
    aboutText->setText(
        "<h2>CF Dojo</h2>"
        "<p>Local competitive programming IDE for fast iteration and testing.</p>"
        "<h3>Quick start</h3>"
        "<ol>"
        "<li>Open a .cpack or import from Competitive Companion.</li>"
        "<li>Write your solution in solution.cpp.</li>"
        "<li>Add tests and click Run All.</li>"
        "</ol>"
        "<h3>Documentation</h3>"
        "<p>See docs/quickstart.md for the full guide.</p>"
        "<h3>Credits</h3>"
        "<p>Icon sources (The Noun Project). License: CC BY 3.0</p>"
        "<ul>"
        "<li>Template - Mamank - <a href=\"https://thenounproject.com/icon/template-8113543/\">source</a></li>"
        "<li>Trash - insdesign86 - <a href=\"https://thenounproject.com/icon/trash-4665730/\">source</a></li>"
        "<li>Test case - SBTS - <a href=\"https://thenounproject.com/icon/test-case-2715499/\">source</a></li>"
        "<li>Anvil - Alum Design - <a href=\"https://thenounproject.com/icon/anvil-8089762/\">source</a></li>"
        "<li>Load testing - Happy Girl - <a href=\"https://thenounproject.com/icon/load-testing-6394477/\">source</a></li>"
        "<li>Copy - Kosong Tujuh - <a href=\"https://thenounproject.com/icon/copy-5457986/\">source</a></li>"
        "<li>File - Damar Creative - <a href=\"https://thenounproject.com/icon/8251834/\">source</a></li>"
        "<li>Target - Radhika Studio - <a href=\"https://thenounproject.com/icon/8090227/\">source</a></li>"
        "<li>Settings - Alzam - <a href=\"https://thenounproject.com/icon/5079171/\">source</a></li>"
        "<li>Battle - Zach Bogart - <a href=\"https://thenounproject.com/icon/5248405/\">source</a></li>"
        "</ul>");
    scrollLayout->addWidget(aboutText);
    scrollLayout->addStretch();

    scrollContent->setLayout(scrollLayout);
    scrollArea->setWidget(scrollContent);
    scrollArea->setStyleSheet(
        QString("QScrollArea { background: %1; border: 1px solid %2; }"
                "QScrollArea > QWidget > QWidget { background: %1; }")
            .arg(panelBg, panelEdge));
    contentLayout->addWidget(scrollArea, 1);

    auto *buttonRow = new QWidget(contentPanel);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(8);
    buttonLayout->addStretch();
    auto *closeBtn = new QPushButton("Close", buttonRow);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(closeBtn);
    contentLayout->addWidget(buttonRow);

    layout->addWidget(contentPanel, 1);
    dialog.exec();
}

void MainWindow::showCopyToast() {
    if (!copyToast_) {
        copyToast_ = new QWidget(this);
        copyToast_->setObjectName("CopyToast");
        copyToast_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

        auto *layout = new QHBoxLayout(copyToast_);
        layout->setContentsMargins(12, 6, 12, 6);
        layout->setSpacing(6);

        copyToastLabel_ = new QLabel(copyToast_);
        copyToastLabel_->setObjectName("CopyToastLabel");
        layout->addWidget(copyToastLabel_);

        copyToastTimer_ = new QTimer(copyToast_);
        copyToastTimer_->setSingleShot(true);
        connect(copyToastTimer_, &QTimer::timeout, copyToast_, &QWidget::hide);
    }

    copyToastLabel_->setText("Answer copied to clipboard.");
    copyToast_->adjustSize();

    const int margin = 16;
    const QRect area = rect();
    const int x = area.right() - copyToast_->width() - margin;
    const int y = area.bottom() - copyToast_->height() - margin;
    copyToast_->move(std::max(margin, x), std::max(margin, y));

    copyToast_->show();
    copyToast_->raise();
    copyToastTimer_->start(1600);
}

void MainWindow::syncEditorToMode() {
    if (!codeEditor_) {
        return;
    }
    const QString text = codeEditor_->text();
    switch (editorMode_) {
    case EditorMode::Solution:
        currentSolutionCode_ = text;
        break;
    case EditorMode::Brute:
        currentBruteCode_ = text;
        break;
    case EditorMode::Generator:
        currentGeneratorCode_ = text;
        break;
    case EditorMode::Template:
        currentTemplate_ = text;
        break;
    case EditorMode::Problem:
        if (text != currentProblemRaw_) {
            currentProblemRaw_ = text;
            problemEdited_ = true;
            QJsonParseError parseError;
            QJsonDocument doc = QJsonDocument::fromJson(
                currentProblemRaw_.toUtf8(), &parseError);
            if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
                currentProblem_ = doc.object();
            }
            updateProblemMetaUi();
        }
        break;
    case EditorMode::Testcases:
        if (text != currentTestcasesRaw_) {
            currentTestcasesRaw_ = text;
            testcasesEdited_ = true;
        }
        break;
    }
}

void MainWindow::updateEditorModeButtons() {
    if (cpackTree_ && cpackModel_ && cpackTree_->selectionModel()) {
        QSignalBlocker blocker(cpackTree_->selectionModel());
        QModelIndex match;
        for (int row = 0; row < cpackModel_->rowCount(); ++row) {
            QStandardItem *item = cpackModel_->item(row);
            if (!item) {
                continue;
            }
            if (item->data(Qt::UserRole).toInt() == static_cast<int>(editorMode_)) {
                match = item->index();
                break;
            }
        }
        if (match.isValid()) {
            cpackTree_->setCurrentIndex(match);
        } else {
            cpackTree_->clearSelection();
        }
    }
}

void MainWindow::updateProblemMetaUi() {
    if (!testPanelWidgets_.metaLabel) {
        return;
    }
    const int timeLimit = currentProblem_.value("timeLimit").toInt(0);
    const int memoryLimit = currentProblem_.value("memoryLimit").toInt(0);

    double secondsValue = 0.0;
    if (timeLimit > 0) {
        secondsValue = timeLimit / 1000.0;
        const int secondsRounded = std::max(1, static_cast<int>(std::round(secondsValue)));
        if (currentTimeout_ != secondsRounded) {
            currentTimeout_ = secondsRounded;
        }
    }

    const QString timeText = timeLimit > 0
        ? QString("%1 s").arg(secondsValue, 0, 'f', 2)
        : QString();
    const QString memoryText = memoryLimit > 0
        ? QString("%1 MB").arg(memoryLimit)
        : QString();

    testPanelWidgets_.metaLabel->setText(
        QString("TL - %1  ML - %2")
            .arg(timeText, memoryText));

}

bool MainWindow::confirmDiscardUnsaved(const QString &actionLabel) {
    if (!isDirty_) {
        return true;
    }

    QMessageBox box(this);
    box.setWindowTitle("Unsaved Changes");
    box.setIcon(QMessageBox::Warning);
    box.setText("You have unsaved changes in this problem.");
    box.setInformativeText(QString("Save before %1?").arg(actionLabel));
    auto *saveBtn = box.addButton("Save", QMessageBox::AcceptRole);
    auto *discardBtn = box.addButton("Discard", QMessageBox::DestructiveRole);
    box.addButton("Cancel", QMessageBox::RejectRole);
    box.setDefaultButton(saveBtn);
    box.exec();

    if (box.clickedButton() == saveBtn) {
        saveFile();
        return !isDirty_;
    }
    if (box.clickedButton() == discardBtn) {
        setDirty(false);
        return true;
    }
    return false;
}

void MainWindow::markDirty() {
    if (dirtySuppressionDepth_ > 0) {
        return;
    }
    setDirty(true);
    scheduleAutosave();
}

void MainWindow::setDirty(bool dirty) {
    if (isDirty_ == dirty) {
        return;
    }
    isDirty_ = dirty;
    updateWindowTitle();
    if (!dirty) {
        clearAutosaveFiles();
    }
}

void MainWindow::updateWindowTitle() {
    QString modeLabel;
    switch (editorMode_) {
    case EditorMode::Solution:
        modeLabel = "solution.cpp";
        break;
    case EditorMode::Brute:
        modeLabel = "brute.cpp";
        break;
    case EditorMode::Generator:
        modeLabel = "generator.cpp";
        break;
    case EditorMode::Template:
        modeLabel = "template.cpp";
        break;
    case EditorMode::Problem:
        modeLabel = "problem.json";
        break;
    case EditorMode::Testcases:
        modeLabel = "testcases.json";
        break;
    }
    const QString editLabel = hasSavedFile_ ? modeLabel : "Untitled";
    const QString dirtyPrefix = isDirty_ ? "* " : "";
    setWindowTitle(QString("%1%2 - %3").arg(dirtyPrefix, editLabel, baseWindowTitle_));
}

QString MainWindow::autosaveDir() const {
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.cfdojo";
    }
    QDir dir(base);
    dir.mkpath(".");
    const QString autosaveRoot = dir.filePath("autosave");
    QDir autosaveDir(autosaveRoot);
    autosaveDir.mkpath(".");
    return autosaveRoot;
}

QString MainWindow::autosaveCpackPath() const {
    return QDir(autosaveDir()).filePath("autosave.cpack");
}

QString MainWindow::autosaveMetaPath() const {
    return QDir(autosaveDir()).filePath("autosave.json");
}

QString MainWindow::autosaveSessionPath() const {
    return QDir(autosaveDir()).filePath("session.lock");
}

void MainWindow::setupAutosave() {
    if (!autosaveTimer_) {
        autosaveTimer_ = new QTimer(this);
        autosaveTimer_->setSingleShot(true);
        connect(autosaveTimer_, &QTimer::timeout, this, &MainWindow::performAutosave);
    }

    QFile sessionFile(autosaveSessionPath());
    if (sessionFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString stamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
        sessionFile.write(stamp.toUtf8());
        sessionFile.close();
    }
}

void MainWindow::scheduleAutosave() {
    if (!autosaveTimer_ || !hasSavedFile_) {
        return;
    }
    autosaveTimer_->start(std::max(1000, autosaveIntervalMs_));
}

void MainWindow::performAutosave() {
    if (!isDirty_ || !hasSavedFile_) {
        return;
    }

    syncEditorToMode();
    CpackFileHandler handler;
    handler.addFile("soluiton.cpp", currentSolutionCode_.toUtf8());
    handler.addFile("brute.cpp", currentBruteCode_.toUtf8());
    handler.addFile("generator.cpp", currentGeneratorCode_.toUtf8());
    handler.addFile("template.cpp", currentTemplate_.toUtf8());

    if (problemEdited_) {
        handler.addFile("problem.json", currentProblemRaw_.toUtf8());
    } else if (!currentProblem_.isEmpty()) {
        QJsonDocument problemDoc(currentProblem_);
        handler.addFile("problem.json", problemDoc.toJson(QJsonDocument::Indented));
    }

    if (testcasesEdited_) {
        handler.addFile("testcases.json", currentTestcasesRaw_.toUtf8());
    } else {
        QJsonArray testsArray;
        for (const auto &widgets : caseWidgets_) {
            QJsonObject testObj;
            if (widgets.inputEditor) {
                testObj["input"] = widgets.inputEditor->toPlainText();
            }
            if (widgets.expectedEditor) {
                testObj["output"] = widgets.expectedEditor->toPlainText();
            }
            testsArray.append(testObj);
        }
        if (!testsArray.isEmpty()) {
            QJsonObject testsDoc;
            testsDoc["tests"] = testsArray;
            testsDoc["timeout"] = currentTimeout_;
            QJsonDocument doc(testsDoc);
            handler.addFile("testcases.json", doc.toJson(QJsonDocument::Indented));
        }
    }

    if (!handler.save(autosaveCpackPath())) {
        return;
    }

    QJsonObject meta;
    meta["timestamp"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    meta["filePath"] = currentFilePath_;
    meta["editorMode"] = static_cast<int>(editorMode_);
    meta["dirty"] = true;
    meta["hasSavedFile"] = hasSavedFile_;
    QJsonDocument metaDoc(meta);
    QFile metaFile(autosaveMetaPath());
    if (metaFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        metaFile.write(metaDoc.toJson(QJsonDocument::Indented));
        metaFile.close();
    }
}

void MainWindow::clearAutosaveFiles() {
    QFile::remove(autosaveCpackPath());
    QFile::remove(autosaveMetaPath());
}

void MainWindow::checkForRecovery() {
    const QString cpackPath = autosaveCpackPath();
    const QString metaPath = autosaveMetaPath();
    if (!QFile::exists(cpackPath) || !QFile::exists(metaPath)) {
        return;
    }

    QFile metaFile(metaPath);
    if (!metaFile.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument metaDoc = QJsonDocument::fromJson(metaFile.readAll());
    metaFile.close();
    if (!metaDoc.isObject()) {
        return;
    }

    const QJsonObject meta = metaDoc.object();
    const bool wasDirty = meta.value("dirty").toBool(false);
    const bool crashed = QFile::exists(autosaveSessionPath());
    if (!wasDirty && !crashed) {
        return;
    }

    QString timeLabel = meta.value("timestamp").toString();
    if (!timeLabel.isEmpty()) {
        const QDateTime stamp = QDateTime::fromString(timeLabel, Qt::ISODate).toLocalTime();
        if (stamp.isValid()) {
            timeLabel = stamp.toString("yyyy-MM-dd HH:mm");
        }
    }

    QMessageBox box(this);
    box.setWindowTitle("Recover Unsaved Work");
    box.setIcon(QMessageBox::Question);
    box.setWindowModality(Qt::WindowModal);
    box.setText("A recovery file was found from a previous session.");
    if (!timeLabel.isEmpty()) {
        box.setInformativeText(QString("Last autosave: %1").arg(timeLabel));
    }
    auto *restoreBtn = box.addButton("Restore", QMessageBox::AcceptRole);
    auto *discardBtn = box.addButton("Discard", QMessageBox::DestructiveRole);
    box.setDefaultButton(restoreBtn);
    QTimer::singleShot(0, &box, [this, &box]() {
        box.adjustSize();
        const QSize size = box.size();
        const QRect parentRect = frameGeometry();
        QPoint pos(parentRect.center().x() - size.width() / 2,
                   parentRect.center().y() - size.height() / 2);
        QScreen *screen = nullptr;
        if (windowHandle()) {
            screen = windowHandle()->screen();
        }
        if (!screen) {
            screen = QGuiApplication::screenAt(parentRect.center());
        }
        if (!screen) {
            screen = this->screen();
        }
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        if (screen) {
            const QRect bounds = screen->availableGeometry();
            pos.setX(std::clamp(pos.x(), bounds.left(), bounds.right() - size.width()));
            pos.setY(std::clamp(pos.y(), bounds.top(), bounds.bottom() - size.height()));
        }
        box.move(pos);
        box.raise();
        box.activateWindow();
    });
    box.exec();

    if (box.clickedButton() == restoreBtn) {
        CpackFileHandler handler;
        if (!handler.load(cpackPath)) {
            QMessageBox::critical(this, "Error",
                "Failed to restore recovery file: " + handler.errorString());
            return;
        }
        const QString savedPath = meta.value("filePath").toString();
        const bool savedFlag = meta.value("hasSavedFile").toBool(!savedPath.isEmpty());
        loadCpackFromHandler(handler, savedPath, savedFlag);

        const int modeValue = meta.value("editorMode").toInt(
            static_cast<int>(EditorMode::Solution));
        setEditorMode(static_cast<EditorMode>(modeValue));
        setDirty(true);
        scheduleAutosave();
    } else if (box.clickedButton() == discardBtn) {
        clearAutosaveFiles();
    }
}

void MainWindow::loadCpackFromHandler(const CpackFileHandler &handler,
                                      const QString &path,
                                      bool markSavedFile) {
    DirtyScope guard(this);
    if (handler.hasFile("manifest.json")) {
        QJsonParseError manifestError;
        const QJsonDocument manifestDoc = QJsonDocument::fromJson(
            handler.getFile("manifest.json"), &manifestError);
        if (manifestError.error == QJsonParseError::NoError && manifestDoc.isObject()) {
            const int version = manifestDoc.object().value("version").toInt(1);
            if (version > 1) {
                QMessageBox::critical(this, "Error", "Unsupported CPack format");
                return;
            }
        }
    }

    const bool hasSolutionFile =
        handler.hasFile("soluiton.cpp") || handler.hasFile("solution.cpp");
    if (!hasSolutionFile) {
        QMessageBox::critical(this, "Error", "Unsupported CPack format");
        return;
    }

    currentSolutionCode_.clear();
    if (handler.hasFile("soluiton.cpp")) {
        currentSolutionCode_ = QString::fromUtf8(handler.getFile("soluiton.cpp"));
    } else if (handler.hasFile("solution.cpp")) {
        currentSolutionCode_ = QString::fromUtf8(handler.getFile("solution.cpp"));
    }
    if (codeEditor_) {
        codeEditor_->setText(currentSolutionCode_);
    }

    currentBruteCode_.clear();
    currentGeneratorCode_.clear();
    if (handler.hasFile("brute.cpp")) {
        currentBruteCode_ = QString::fromUtf8(handler.getFile("brute.cpp"));
    }
    if (handler.hasFile("generator.cpp")) {
        currentGeneratorCode_ = QString::fromUtf8(handler.getFile("generator.cpp"));
    }

    editorMode_ = EditorMode::Solution;
    updateEditorModeButtons();
    updateWindowTitle();

    if (handler.hasFile("template.cpp")) {
        currentTemplate_ = QString::fromUtf8(handler.getFile("template.cpp"));
    } else {
        currentTemplate_ = loadDefaultTemplate();
    }

    problemEdited_ = false;
    if (handler.hasFile("problem.json")) {
        const QByteArray problemBytes = handler.getFile("problem.json");
        currentProblemRaw_ = QString::fromUtf8(problemBytes);
        QJsonParseError parseError;
        QJsonDocument problemDoc = QJsonDocument::fromJson(problemBytes, &parseError);
        if (parseError.error == QJsonParseError::NoError) {
            currentProblem_ = problemDoc.object();
        }
    } else {
        currentProblem_ = QJsonObject();
        currentProblemRaw_.clear();
    }
    const QString loadedTitle = currentProblem_.value("name").toString().trimmed();
    if (!loadedTitle.isEmpty()) {
        baseWindowTitle_ = QString("CF Dojo - %1").arg(loadedTitle);
    } else {
        baseWindowTitle_ = "CF Dojo - v1.0";
    }
    updateProblemMetaUi();

    testcasesEdited_ = false;
    currentTimeout_ = 5;
    bool loadedTests = false;
    for (auto &widgets : caseWidgets_) {
        if (widgets.panel) {
            widgets.panel->deleteLater();
        }
    }
    caseWidgets_.clear();
    testCases_.clear();

    if (handler.hasFile("testcases.json")) {
        const QByteArray testsBytes = handler.getFile("testcases.json");
        currentTestcasesRaw_ = QString::fromUtf8(testsBytes);
        QJsonParseError parseError;
        QJsonDocument testsDoc = QJsonDocument::fromJson(testsBytes, &parseError);
        if (parseError.error == QJsonParseError::NoError && testsDoc.isObject()) {
            QJsonObject testsObj = testsDoc.object();
            QJsonArray testsArray = testsObj["tests"].toArray();

            if (testsObj.contains("timeout")) {
                currentTimeout_ = testsObj["timeout"].toInt(5);
            }

            for (const QJsonValue &testVal : testsArray) {
                QJsonObject test = testVal.toObject();
                addTestCase();
                if (!caseWidgets_.empty()) {
                    auto &lastCase = caseWidgets_.back();
                    if (lastCase.inputEditor) {
                        lastCase.inputEditor->setPlainText(test["input"].toString());
                    }
                    if (lastCase.expectedEditor) {
                        lastCase.expectedEditor->setPlainText(test["output"].toString());
                    }
                }
            }
            loadedTests = !testsArray.isEmpty();
        }
    } else {
        currentTestcasesRaw_.clear();
    }

    if (!loadedTests) {
        addTestCase();
    }

    currentFilePath_ = path;
    hasSavedFile_ = markSavedFile;
    setDirty(false);
    setPlainTextMode(false);
    updateTemplateAvailability();
    updateEditorModeButtons();
    updateWindowTitle();
}

QString MainWindow::buildProblemJson() const {
    if (currentProblem_.isEmpty()) {
        return QString();
    }
    QJsonDocument doc(currentProblem_);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

QString MainWindow::buildTestcasesJson() const {
    QJsonArray testsArray;
    for (const auto &widgets : caseWidgets_) {
        QJsonObject testObj;
        if (widgets.inputEditor) {
            testObj["input"] = widgets.inputEditor->toPlainText();
        }
        if (widgets.expectedEditor) {
            testObj["output"] = widgets.expectedEditor->toPlainText();
        }
        testsArray.append(testObj);
    }

    QJsonObject testsDoc;
    testsDoc["tests"] = testsArray;
    testsDoc["timeout"] = currentTimeout_;
    QJsonDocument doc(testsDoc);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}


void MainWindow::populateCpackTree() {
    if (!cpackModel_) {
        return;
    }

    cpackModel_->clear();
    const QIcon fileIcon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);
    auto addCpackItem = [this, &fileIcon](const QString &label,
                                          const QString &tooltip,
                                          EditorMode mode) {
        auto *item = new QStandardItem(fileIcon, label);
        item->setData(static_cast<int>(mode), Qt::UserRole);
        item->setEditable(false);
        item->setToolTip(tooltip);
        cpackModel_->appendRow(item);
        return item;
    };

    addCpackItem("solution.cpp", "Your main solution code.", EditorMode::Solution);
    addCpackItem("brute.cpp", "Optional: slow but correct solution for stress tests.",
                 EditorMode::Brute);
    addCpackItem("generator.cpp", "Optional: randomized generator for stress tests.",
                 EditorMode::Generator);
    cpackTemplateItem_ = addCpackItem("template.cpp",
                                      "Template with //#main where solution is inserted.",
                                      EditorMode::Template);
    addCpackItem("problem.json", "Problem metadata from Competitive Companion.",
                 EditorMode::Problem);
    addCpackItem("testcases.json", "Input/output test cases and timeout.",
                 EditorMode::Testcases);
}

void MainWindow::setEditorMode(EditorMode mode) {
    if (mode == EditorMode::Template && !transcludeTemplateEnabled_) {
        return;
    }
    if (editorMode_ != mode) {
        syncEditorToMode();
        editorMode_ = mode;
        if (codeEditor_) {
            QString next;
            switch (editorMode_) {
            case EditorMode::Solution:
                next = currentSolutionCode_;
                break;
            case EditorMode::Brute:
                next = currentBruteCode_;
                break;
            case EditorMode::Generator:
                next = currentGeneratorCode_;
                break;
            case EditorMode::Template:
                next = currentTemplate_;
                break;
            case EditorMode::Problem:
                if (!problemEdited_) {
                    if (currentProblemRaw_.isEmpty()) {
                        currentProblemRaw_ = buildProblemJson();
                    }
                }
                next = currentProblemRaw_;
                break;
            case EditorMode::Testcases:
                if (!testcasesEdited_) {
                    if (currentTestcasesRaw_.isEmpty()) {
                        currentTestcasesRaw_ = buildTestcasesJson();
                    }
                }
                next = currentTestcasesRaw_;
                break;
            }
            DirtyScope guard(this);
            codeEditor_->setText(next);
            updateTemplateMarkerVisibility();
        }
    }
    updateEditorModeButtons();
    updateWindowTitle();
}

void MainWindow::onSidePanelCollapsedChanged(bool collapsed) {
    if (sidebarToggle_) {
        sidebarToggle_->setChecked(!collapsed);
    }
    updateActivityBarActiveStates(collapsed);
}

void MainWindow::updateActivityBarActiveStates(bool collapsed) {
    if (collapsed) {
        if (sidebarToggle_) {
            sidebarToggle_->setActiveState(false);
        }
        if (newFileButton_) {
            newFileButton_->setActiveState(false);
        }
        if (stressTestButton_) {
            stressTestButton_->setActiveState(false);
        }
        if (templateButton_) {
            templateButton_->setActiveState(false);
        }
        return;
    }
    if (sideStack_ && sideStack_->currentWidget() == fileExplorer_) {
        if (newFileButton_) {
            newFileButton_->setActiveState(true);
        }
        if (sidebarToggle_) {
            sidebarToggle_->setActiveState(false);
        }
        if (stressTestButton_) {
            stressTestButton_->setActiveState(false);
        }
        if (templateButton_) {
            templateButton_->setActiveState(false);
        }
        return;
    }
    if (sideStack_ && sideStack_->currentWidget() == stressTestPanel_) {
        if (stressTestButton_) {
            stressTestButton_->setActiveState(true);
        }
        if (sidebarToggle_) {
            sidebarToggle_->setActiveState(false);
        }
        if (newFileButton_) {
            newFileButton_->setActiveState(false);
        }
        if (templateButton_) {
            templateButton_->setActiveState(false);
        }
        return;
    }
    if (sideStack_ && sideStack_->currentWidget() == cpackPanel_) {
        if (templateButton_) {
            templateButton_->setActiveState(true);
        }
        if (sidebarToggle_) {
            sidebarToggle_->setActiveState(false);
        }
        if (newFileButton_) {
            newFileButton_->setActiveState(false);
        }
        if (stressTestButton_) {
            stressTestButton_->setActiveState(false);
        }
        return;
    }
    if (sidebarToggle_) {
        sidebarToggle_->setActiveState(true);
    }
    if (newFileButton_) {
        newFileButton_->setActiveState(false);
    }
    if (stressTestButton_) {
        stressTestButton_->setActiveState(false);
    }
    if (templateButton_) {
        templateButton_->setActiveState(false);
    }
}

void MainWindow::updateTemplateAvailability() {
    if (cpackTemplateItem_) {
        if (cpackTree_) {
            const int row = cpackTemplateItem_->row();
            cpackTree_->setRowHidden(row, QModelIndex(), !transcludeTemplateEnabled_);
        }
    }
    if (!transcludeTemplateEnabled_ && editorMode_ == EditorMode::Template) {
        setEditorMode(EditorMode::Solution);
    }
    updateTemplateMarkerVisibility();
    syncTemplateToggleUi();
}

void MainWindow::updateTemplateMarkerVisibility() {
    if (!codeEditor_) {
        return;
    }

    const int lineCount = codeEditor_->lines();
    if (lineCount <= 0) {
        return;
    }

    codeEditor_->SendScintilla(QsciScintilla::SCI_SHOWLINES, 0, lineCount - 1);

    if (!transcludeTemplateEnabled_ || editorMode_ != EditorMode::Template) {
        return;
    }

    const QString text = codeEditor_->text();
    if (!text.contains(kTemplateMarker)) {
        return;
    }

    const QStringList lines = text.split('\n', Qt::KeepEmptyParts);
    const int maxLine = std::min(lineCount, static_cast<int>(lines.size())) - 1;
    if (maxLine < 0) {
        return;
    }

    int hideStart = -1;
    for (int i = 0; i <= maxLine; ++i) {
        const bool hasMarker = lines[i].contains(kTemplateMarker);
        if (hasMarker) {
            if (hideStart == -1) {
                hideStart = i;
            }
        } else if (hideStart != -1) {
            codeEditor_->SendScintilla(QsciScintilla::SCI_HIDELINES, hideStart, i - 1);
            hideStart = -1;
        }
    }
    if (hideStart != -1) {
        codeEditor_->SendScintilla(QsciScintilla::SCI_HIDELINES, hideStart, maxLine);
    }
}

void MainWindow::syncTemplateToggleUi() {
    const QString menuTip = transcludeTemplateEnabled_
        ? "Hide template view"
        : "Show template view";
    const QString barTip = transcludeTemplateEnabled_
        ? "Hide template"
        : "Show template";

    if (menuTemplateButton_) {
        menuTemplateButton_->setCheckable(true);
        menuTemplateButton_->setChecked(transcludeTemplateEnabled_);
        menuTemplateButton_->setToolTip(menuTip);
    }
    if (templateButton_) {
        templateButton_->setToolTip(barTip);
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
    if (testPanelWidgets_.addButton) {
        const int addIndex = testPanelWidgets_.casesLayout->indexOf(testPanelWidgets_.addButton);
        if (addIndex >= 0) {
            insertIndex = addIndex;
        }
    }
    testPanelWidgets_.casesLayout->insertWidget(insertIndex, widgets.panel);
    caseWidgets_.push_back(widgets);
    testCases_.push_back({});

    auto markDirtyOnChange = [this]() {
        markDirty();
    };
    if (widgets.inputEditor) {
        connect(widgets.inputEditor, &QPlainTextEdit::textChanged,
                this, markDirtyOnChange);
    }
    if (widgets.expectedEditor) {
        connect(widgets.expectedEditor, &QPlainTextEdit::textChanged,
                this, markDirtyOnChange);
    }

    // Connect run button
    if (widgets.runButton) {
        connect(widgets.runButton, &QPushButton::clicked, this,
                [this, button = widgets.runButton]() {
            if (executionController_ &&
                executionController_->state() != ExecutionController::State::Idle) {
                if (runAllSequentialActive_) {
                    cancelSequentialRunAll();
                }
                executionController_->stop();
                return;
            }

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

            // Set template for transclusion before running
            executionController_->setTemplate(currentTemplate_);
            executionController_->setTimeoutMs(currentTimeout_ * 1000);
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
    markDirty();
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
    markDirty();
}

void MainWindow::clearAllTestCases() {
    for (auto &widgets : caseWidgets_) {
        if (widgets.panel) {
            widgets.panel->deleteLater();
        }
    }
    caseWidgets_.clear();
    testCases_.clear();
    markDirty();
}

void MainWindow::runNextSequentialTest() {
    if (!runAllSequentialActive_) {
        return;
    }

    while (!runAllQueue_.empty()) {
        const int nextIndex = runAllQueue_.front();
        runAllQueue_.pop_front();

        if (nextIndex < 0 || nextIndex >= static_cast<int>(caseWidgets_.size())) {
            continue;
        }

        runAllCurrentIndex_ = nextIndex;

        TestCase &testCase = testCases_.at(static_cast<size_t>(nextIndex));
        const auto &widgets = caseWidgets_.at(static_cast<size_t>(nextIndex));

        if (widgets.inputEditor) {
            testCase.input = widgets.inputEditor->toPlainText();
        }
        if (widgets.expectedEditor) {
            testCase.expectedOutput = widgets.expectedEditor->toPlainText();
        }

        executionController_->setTemplate(currentTemplate_);
        executionController_->setTimeoutMs(currentTimeout_ * 1000);
        executionController_->runWithBindings(makeBindings(widgets));
        return;
    }

    cancelSequentialRunAll();
}

void MainWindow::cancelSequentialRunAll() {
    runAllSequentialActive_ = false;
    runAllQueue_.clear();
    runAllCurrentIndex_ = -1;
}

void MainWindow::applyCompileErrorToAllCases(const QString &error) {
    const QString trimmedError = error.trimmed();
    const bool showError = !trimmedError.isEmpty();

    for (auto &widgets : caseWidgets_) {
        if (widgets.statusLabel) {
            widgets.statusLabel->setText("CE");
            widgets.statusLabel->setStyleSheet("color: #c42b1c; font-weight: 700;");
        }

        if (widgets.outputViewer) {
            widgets.outputViewer->clear();
        }
        if (widgets.errorViewer) {
            widgets.errorViewer->setPlainText(trimmedError);
        }

        if (widgets.outputBlock) {
            widgets.outputBlock->setVisible(false);
        }
        if (widgets.errorBlock) {
            widgets.errorBlock->setVisible(showError);
        }
        if (widgets.outputSplitter) {
            widgets.outputSplitter->setVisible(showError);
            if (showError) {
                widgets.outputSplitter->setSizes({0, 1});
            }
        }
    }
}

void MainWindow::runStressTest() {
    if (stressRunning_) {
        return;
    }
    if (!codeEditor_) {
        return;
    }

    syncEditorToMode();

    const QString solution = currentSolutionCode_;
    const QString brute = currentBruteCode_;
    const QString generator = currentGeneratorCode_;

    int count = 1;
    if (stressCountEdit_) {
        bool ok = false;
        const int parsed = stressCountEdit_->text().trimmed().toInt(&ok);
        if (ok) {
            count = std::clamp(parsed, 1, 10000);
        }
    }
    QString tmpl = currentTemplate_;
    if (tmpl.isEmpty()) {
        tmpl = "//#main";
    }
    const bool requiresSource = !tmpl.contains("//#main");
    if (requiresSource &&
        (solution.trimmed().isEmpty() ||
         brute.trimmed().isEmpty() ||
         generator.trimmed().isEmpty())) {
        if (stressStatusLabel_) {
            stressStatusLabel_->setText("Missing code");
            stressStatusLabel_->setStyleSheet("color: #c42b1c; font-weight: 700;");
        }
        if (stressComplexityLabel_) {
            stressComplexityLabel_->setText("Suspected: insufficient timing data");
            stressComplexityLabel_->setVisible(true);
        }
        if (stressLog_) {
            stressLog_->setPlainText(
                "Please provide solution.cpp, brute.cpp, and generator.cpp before running stress test.");
        }
        return;
    }

    if (!stressWatcher_) {
        stressWatcher_ = new QFutureWatcher<StressResult>(this);
        connect(stressWatcher_, &QFutureWatcher<StressResult>::finished, this, [this]() {
            stressRunning_ = false;
            if (stressRunButton_) {
                stressRunButton_->setEnabled(true);
            }

            const StressResult result = stressWatcher_->result();
            if (stressComplexityLabel_) {
                if (!result.complexity.isEmpty()) {
                    stressComplexityLabel_->setText(result.complexity);
                    stressComplexityLabel_->setVisible(true);
                } else {
                    stressComplexityLabel_->setVisible(false);
                    stressComplexityLabel_->clear();
                }
            }

            if (!result.error.isEmpty()) {
                if (stressStatusLabel_) {
                    stressStatusLabel_->setText("Error");
                    stressStatusLabel_->setStyleSheet("color: #c42b1c; font-weight: 700;");
                }
                if (stressLog_) {
                    stressLog_->setPlainText(result.error);
                }
                return;
            }

            if (result.passed) {
                if (stressStatusLabel_) {
                    stressStatusLabel_->setText("Passed");
                    stressStatusLabel_->setStyleSheet("color: #2e7d32; font-weight: 700;");
                }
                if (stressComplexityLabel_) {
                    if (!result.complexity.isEmpty()) {
                        stressComplexityLabel_->setText(result.complexity);
                        stressComplexityLabel_->setVisible(true);
                    } else {
                        stressComplexityLabel_->setVisible(false);
                        stressComplexityLabel_->clear();
                    }
                }
                if (stressLog_) {
                    QString summary = QString("All %1 testcases passed.").arg(result.totalCount);
                    stressLog_->setPlainText(summary);
                }
                return;
            }

            if (stressStatusLabel_) {
                stressStatusLabel_->setText("Wrong Answer");
                stressStatusLabel_->setStyleSheet("color: #c42b1c; font-weight: 700;");
            }
            if (stressLog_) {
                QString details;
                details += QString("Mismatch at test #%1\n").arg(result.failedIndex + 1);
                details += "\nInput:\n";
                details += result.input;
                details += "\n\nBrute output:\n";
                details += result.expected;
                details += "\n\nSolution output:\n";
                details += result.actual;
                if (!result.stderrOutput.isEmpty()) {
                    details += "\n\nStderr:\n";
                    details += result.stderrOutput;
                }
                stressLog_->setPlainText(details);
            }
        });
    }

    stressRunning_ = true;
    if (stressRunButton_) {
        stressRunButton_->setEnabled(false);
    }
    if (stressStatusLabel_) {
        stressStatusLabel_->setText("Running...");
        stressStatusLabel_->setStyleSheet("font-weight: 700;");
    }
    if (stressLog_) {
        stressLog_->setPlainText(QString("Running %1 testcases...").arg(count));
    }
    if (stressComplexityLabel_) {
        stressComplexityLabel_->setVisible(false);
        stressComplexityLabel_->clear();
    }

    const int timeoutMs = currentTimeout_ * 1000;
    const QString solutionCopy = solution;
    const QString bruteCopy = brute;
    const QString generatorCopy = generator;
    const QString tmplCopy = tmpl;

    const bool useParallel = multithreadingEnabled_;
    stressWatcher_->setFuture(QtConcurrent::run([this, count, solutionCopy, bruteCopy, generatorCopy, tmplCopy, timeoutMs, useParallel]() {
        return runStressTestWorker(count, solutionCopy, bruteCopy, generatorCopy, tmplCopy, timeoutMs, useParallel);
    }));
}

MainWindow::StressResult MainWindow::runStressTestWorker(int count,
                                                        const QString &solution,
                                                        const QString &brute,
                                                        const QString &generator,
                                                        const QString &tmpl,
                                                        int timeoutMs,
                                                        bool parallel) const {
    StressResult result;
    result.totalCount = count;

    std::vector<double> inputSizes;
    std::vector<double> solutionTimes;
    inputSizes.reserve(static_cast<size_t>(count));
    solutionTimes.reserve(static_cast<size_t>(count));

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        result.error = "Failed to create temporary directory for stress testing.";
        result.complexity = QString("Suspected: insufficient timing data");
        return result;
    }

    auto applyTransclusion = [](const QString &templateText, const QString &solutionText) {
        if (templateText.contains("//#main")) {
            QString replaced = templateText;
            return replaced.replace("//#main", solutionText);
        }
        return solutionText;
    };

    auto compileSource = [&](const QString &code,
                             const QString &sourceName,
                             const QString &exeName,
                             QString *errorOut) -> bool {
        const QString tempPath = tempDir.path();
        const QString sourcePath = QDir(tempPath).filePath(sourceName);
        QFile file(sourcePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorOut) {
                *errorOut = QString("Failed to write %1").arg(sourceName);
            }
            return false;
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << code;
        file.close();

        QStringList args;
        args << "-O2" << "-std=c++17" << sourceName << "-o" << exeName;

        QProcess compiler;
        compiler.setWorkingDirectory(tempPath);
        compiler.start("g++", args);
        if (!compiler.waitForFinished(30000)) {
            if (errorOut) {
                *errorOut = QString("Compilation timed out for %1").arg(sourceName);
            }
            return false;
        }
        if (compiler.exitCode() != 0) {
            const QString err = QString::fromUtf8(compiler.readAllStandardError());
            if (errorOut) {
                *errorOut = err.isEmpty()
                    ? QString("Compilation failed for %1").arg(sourceName)
                    : err;
            }
            return false;
        }
        return true;
    };

    const QString generatorCode = applyTransclusion(tmpl, generator);
    const QString bruteCode = applyTransclusion(tmpl, brute);
    const QString solutionCode = applyTransclusion(tmpl, solution);
    QString compileError;
    if (!compileSource(generatorCode, "generator.cpp", "generator", &compileError)) {
        result.error = QString("Generator compile error:\n%1").arg(compileError);
        result.complexity = QString("Suspected: insufficient timing data");
        return result;
    }
    if (!compileSource(bruteCode, "brute.cpp", "brute", &compileError)) {
        result.error = QString("Brute compile error:\n%1").arg(compileError);
        result.complexity = QString("Suspected: insufficient timing data");
        return result;
    }
    if (!compileSource(solutionCode, "solution.cpp", "solution", &compileError)) {
        result.error = QString("Solution compile error:\n%1").arg(compileError);
        result.complexity = QString("Suspected: insufficient timing data");
        return result;
    }

    auto runProcess = [&](const QString &exePath,
                          const QString &input,
                          const QString &workingDir,
                          QString *stdoutOut,
                          QString *stderrOut,
                          qint64 *timeMs,
                          QString *errorOut) -> bool {
        QProcess process;
        process.setWorkingDirectory(workingDir);
        QElapsedTimer timer;
        timer.start();
        process.start(exePath);
        if (!process.waitForStarted(1000)) {
            if (errorOut) {
                *errorOut = QString("Failed to start %1").arg(exePath);
            }
            return false;
        }
        if (!input.isEmpty()) {
            process.write(input.toUtf8());
        }
        process.closeWriteChannel();

        if (!process.waitForFinished(timeoutMs)) {
            process.kill();
            process.waitForFinished(1000);
            if (errorOut) {
                *errorOut = QString("Time Limit Exceeded: %1").arg(exePath);
            }
            return false;
        }

        if (timeMs) {
            *timeMs = timer.elapsed();
        }

        if (stdoutOut) {
            *stdoutOut = QString::fromUtf8(process.readAllStandardOutput());
        }
        if (stderrOut) {
            *stderrOut = QString::fromUtf8(process.readAllStandardError());
        }

        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            if (errorOut) {
                *errorOut = QString("Runtime Error: %1").arg(exePath);
            }
            return false;
        }
        return true;
    };

    const QString tempPath = tempDir.path();
    const QString generatorExe = QDir(tempPath).filePath("generator");
    const QString bruteExe = QDir(tempPath).filePath("brute");
    const QString solutionExe = QDir(tempPath).filePath("solution");

    std::vector<QString> inputs;
    std::vector<QString> caseDirs;
    inputs.reserve(static_cast<size_t>(count));
    caseDirs.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        const QString caseDir = QDir(tempPath).filePath(QString("case_%1").arg(i + 1));
        QDir().mkpath(caseDir);

        QString input;
        QString stderrOut;
        QString runError;
        if (!runProcess(generatorExe, QString(), caseDir, &input, &stderrOut, nullptr, &runError)) {
            result.error = QString("Generator failed on test #%1:\n%2")
                .arg(i + 1)
                .arg(runError);
            if (!stderrOut.isEmpty()) {
                result.stderrOutput = stderrOut;
            }
            result.complexity = QString("Suspected: insufficient timing data");
            return result;
        }

        inputs.push_back(input);
        caseDirs.push_back(caseDir);
        inputSizes.push_back(static_cast<double>(input.size()));
    }

    if (!parallel) {
        for (int i = 0; i < count; ++i) {
            const QString &input = inputs[static_cast<size_t>(i)];
            const QString &caseDir = caseDirs[static_cast<size_t>(i)];
            QString runError;

            QString bruteOut;
            QString bruteErr;
            if (!runProcess(bruteExe, input, caseDir, &bruteOut, &bruteErr, nullptr, &runError)) {
                result.error = QString("Brute failed on test #%1:\n%2")
                    .arg(i + 1)
                    .arg(runError);
                if (!bruteErr.isEmpty()) {
                    result.stderrOutput = bruteErr;
                }
                result.complexity = suspectedComplexityLabel(inputSizes, solutionTimes);
                return result;
            }

            QString solutionOut;
            QString solutionErr;
            qint64 solutionTime = -1;
            if (!runProcess(solutionExe, input, caseDir, &solutionOut, &solutionErr, &solutionTime, &runError)) {
                result.error = QString("Solution failed on test #%1:\n%2")
                    .arg(i + 1)
                    .arg(runError);
                if (!solutionErr.isEmpty()) {
                    result.stderrOutput = solutionErr;
                }
                result.complexity = suspectedComplexityLabel(inputSizes, solutionTimes);
                return result;
            }

            if (solutionTime > 0) {
                solutionTimes.push_back(static_cast<double>(solutionTime));
            } else {
                solutionTimes.push_back(0.0);
            }

            if (normalizeText(bruteOut) != normalizeText(solutionOut)) {
                result.passed = false;
                result.failedIndex = i;
                result.input = input;
                result.expected = bruteOut;
                result.actual = solutionOut;
                if (!solutionErr.isEmpty()) {
                    result.stderrOutput = solutionErr;
                } else if (!bruteErr.isEmpty()) {
                    result.stderrOutput = bruteErr;
                }
                result.complexity = suspectedComplexityLabel(inputSizes, solutionTimes);
                return result;
            }
        }
    } else {
        struct StressCaseResult {
            int index = -1;
            bool passed = false;
            QString error;
            QString stderrOutput;
            QString input;
            QString expected;
            QString actual;
            qint64 solutionTime = -1;
        };

        QVector<int> indices;
        indices.reserve(count);
        for (int i = 0; i < count; ++i) {
            indices.push_back(i);
        }

        const auto runCase = [&](int index) -> StressCaseResult {
            StressCaseResult caseResult;
            caseResult.index = index;
            const QString &input = inputs[static_cast<size_t>(index)];
            const QString &caseDir = caseDirs[static_cast<size_t>(index)];
            caseResult.input = input;

            QString runError;
            QString bruteOut;
            QString bruteErr;
            if (!runProcess(bruteExe, input, caseDir, &bruteOut, &bruteErr, nullptr, &runError)) {
                caseResult.error = QString("Brute failed on test #%1:\n%2")
                    .arg(index + 1)
                    .arg(runError);
                caseResult.stderrOutput = bruteErr;
                return caseResult;
            }

            QString solutionOut;
            QString solutionErr;
            qint64 solutionTime = -1;
            if (!runProcess(solutionExe, input, caseDir, &solutionOut, &solutionErr, &solutionTime, &runError)) {
                caseResult.error = QString("Solution failed on test #%1:\n%2")
                    .arg(index + 1)
                    .arg(runError);
                caseResult.stderrOutput = solutionErr;
                return caseResult;
            }

            caseResult.solutionTime = solutionTime;
            if (normalizeText(bruteOut) != normalizeText(solutionOut)) {
                caseResult.passed = false;
                caseResult.expected = bruteOut;
                caseResult.actual = solutionOut;
                if (!solutionErr.isEmpty()) {
                    caseResult.stderrOutput = solutionErr;
                } else if (!bruteErr.isEmpty()) {
                    caseResult.stderrOutput = bruteErr;
                }
                return caseResult;
            }

            caseResult.passed = true;
            return caseResult;
        };

        const QVector<StressCaseResult> results = QtConcurrent::blockingMapped(indices, runCase);
        solutionTimes.assign(static_cast<size_t>(count), 0.0);

        int firstFailure = -1;
        StressCaseResult failure;
        for (const auto &caseResult : results) {
            if (caseResult.index >= 0 &&
                caseResult.index < static_cast<int>(solutionTimes.size()) &&
                caseResult.solutionTime > 0) {
                solutionTimes[static_cast<size_t>(caseResult.index)] =
                    static_cast<double>(caseResult.solutionTime);
            }
            if (!caseResult.passed &&
                (firstFailure == -1 || caseResult.index < firstFailure)) {
                firstFailure = caseResult.index;
                failure = caseResult;
            }
        }

        if (firstFailure != -1) {
            result.passed = false;
            result.failedIndex = firstFailure;
            if (!failure.error.isEmpty()) {
                result.error = failure.error;
            }
            result.input = failure.input;
            result.expected = failure.expected;
            result.actual = failure.actual;
            result.stderrOutput = failure.stderrOutput;
            result.complexity = suspectedComplexityLabel(inputSizes, solutionTimes);
            return result;
        }
    }

    result.passed = true;
    result.complexity = suspectedComplexityLabel(inputSizes, solutionTimes);
    return result;
}

QString MainWindow::normalizeText(const QString &text) const {
    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace("\r", "\n");
    QStringList lines = normalized.split('\n');
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty()) {
        lines.removeLast();
    }
    for (QString &line : lines) {
        while (line.endsWith(' ') || line.endsWith('\t')) {
            line.chop(1);
        }
    }
    return lines.join('\n');
}

void MainWindow::runAllTests() {
    if (caseWidgets_.empty() || !codeEditor_) {
        return;
    }

    runAllInputSizes_.clear();
    runAllInputSizes_.reserve(caseWidgets_.size());
    runAllTimesMs_.assign(caseWidgets_.size(), -1.0);
    runAllCollecting_ = true;
    updateTestSummary(QString());

    // Collect all test inputs
    std::vector<TestInput> inputs;
    for (size_t i = 0; i < caseWidgets_.size(); ++i) {
        const auto &widgets = caseWidgets_[i];
        const int inputSize = widgets.inputEditor
            ? widgets.inputEditor->toPlainText().size()
            : 0;
        runAllInputSizes_.push_back(static_cast<double>(inputSize));
        if (multithreadingEnabled_) {
            TestInput input;
            input.testIndex = static_cast<int>(i);
            input.input = widgets.inputEditor ? widgets.inputEditor->toPlainText() : QString();
            input.expectedOutput =
                widgets.expectedEditor ? widgets.expectedEditor->toPlainText() : QString();
            inputs.push_back(input);
        }
        
        // Reset status
        if (widgets.statusLabel) {
            widgets.statusLabel->setText("-");
            widgets.statusLabel->setStyleSheet("font-weight: 700;");
        }
        if (widgets.outputViewer) {
            widgets.outputViewer->clear();
        }
        if (widgets.errorViewer) {
            widgets.errorViewer->clear();
        }
        if (widgets.outputBlock) {
            widgets.outputBlock->setVisible(false);
        }
        if (widgets.errorBlock) {
            widgets.errorBlock->setVisible(false);
        }
        if (widgets.outputSplitter) {
            widgets.outputSplitter->setVisible(false);
        }
    }
    
    if (multithreadingEnabled_) {
        // Use parallel executor
        cancelSequentialRunAll();
        parallelExecutor_->setSourceCode(codeEditor_->text());
        parallelExecutor_->setTemplate(currentTemplate_);
        parallelExecutor_->setTimeout(currentTimeout_ * 1000);
        parallelExecutor_->runAll(inputs);
    } else {
        if (executionController_->state() != ExecutionController::State::Idle) {
            return;
        }

        if (runAllSequentialActive_) {
            return;
        }

        runAllSequentialActive_ = true;
        runAllQueue_.clear();
        for (size_t i = 0; i < caseWidgets_.size(); ++i) {
            runAllQueue_.push_back(static_cast<int>(i));
        }
        runNextSequentialTest();
    }
}

void MainWindow::updateTestCaseTitles() {
    for (size_t i = 0; i < caseWidgets_.size(); ++i) {
        auto *label = caseWidgets_[i].titleLabel;
        if (label) {
            label->setText(QString("TC %1").arg(static_cast<int>(i + 1)));
        }
    }
}

void MainWindow::updateTestSummary(const QString &text) {
    if (!testPanelWidgets_.summaryLabel) {
        return;
    }
    if (text.isEmpty()) {
        testPanelWidgets_.summaryLabel->setVisible(false);
        testPanelWidgets_.summaryLabel->clear();
        return;
    }
    testPanelWidgets_.summaryLabel->setText(text);
    testPanelWidgets_.summaryLabel->setVisible(true);
}

void MainWindow::applyParallelResult(const TestResult &result) {
    if (result.testIndex < 0 ||
        result.testIndex >= static_cast<int>(caseWidgets_.size())) {
        return;
    }

    auto &widgets = caseWidgets_.at(static_cast<size_t>(result.testIndex));
    if (runAllCollecting_ &&
        result.testIndex >= 0 &&
        static_cast<size_t>(result.testIndex) < runAllTimesMs_.size()) {
        runAllTimesMs_[static_cast<size_t>(result.testIndex)] =
            static_cast<double>(result.executionTimeMs);
    }

    if (widgets.outputViewer) {
        widgets.outputViewer->setPlainText(result.output);
        widgets.outputViewer->parentWidget()->show();
    }

    if (widgets.errorViewer) {
        if (!result.error.isEmpty()) {
            widgets.errorViewer->setPlainText(result.error);
            widgets.errorViewer->parentWidget()->show();
        } else {
            widgets.errorViewer->parentWidget()->hide();
        }
    }

        if (widgets.statusLabel) {
            const QString timeSuffix = result.executionTimeMs > 0
                ? QString(" \u2022 %1 ms").arg(result.executionTimeMs)
                : QString();
            const bool isTle = result.error.contains("Time Limit Exceeded");
            if (isTle) {
                widgets.statusLabel->setText("TLE" + timeSuffix);
                widgets.statusLabel->setStyleSheet("color: #c42b1c; font-weight: 700;");
            } else if (result.exitCode != 0 || !result.error.isEmpty()) {
                widgets.statusLabel->setText("Runtime Error" + timeSuffix);
                widgets.statusLabel->setStyleSheet("color: #c42b1c; font-weight: 700;");
            } else if (result.passed) {
                widgets.statusLabel->setText("AC" + timeSuffix);
                widgets.statusLabel->setStyleSheet("color: #2e7d32; font-weight: 700;");
            } else {
                widgets.statusLabel->setText("Wrong Answer" + timeSuffix);
                widgets.statusLabel->setStyleSheet("color: #c42b1c; font-weight: 700;");
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

void MainWindow::newFile() {
    if (!confirmDiscardUnsaved("creating a new file")) {
        return;
    }
    DirtyScope guard(this);
    if (codeEditor_) {
        codeEditor_->clear();
    }
    currentFilePath_.clear();
    hasSavedFile_ = false;
    editorMode_ = EditorMode::Solution;
    currentSolutionCode_.clear();
    currentBruteCode_.clear();
    currentGeneratorCode_.clear();
    currentTemplate_ = loadDefaultTemplate();
    currentProblem_ = QJsonObject();
    currentProblemRaw_.clear();
    currentTestcasesRaw_.clear();
    problemEdited_ = false;
    testcasesEdited_ = false;
    currentTimeout_ = 5;
    updateProblemMetaUi();
    updateEditorModeButtons();
    updateWindowTitle();
    clearAllTestCases();
    addTestCase();
    updateTemplateAvailability();
    updateEditorModeButtons();
    setDirty(false);
    saveFileAsWithTitle("Create CPack");
    setPlainTextMode(false);
}

void MainWindow::openFile() {
    if (!confirmDiscardUnsaved("opening another file")) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open CPack or Text File",
        QString(),
        "All Supported Files (*.cpack *.cpp *.py *.java *.txt *.md *.json *.in *.out);;CPack Files (*.cpack);;All Files (*)"
    );
    
    if (path.isEmpty()) {
        return;
    }
    
    if (path.endsWith(".cpack", Qt::CaseInsensitive)) {
        CpackFileHandler handler;
        if (!handler.load(path)) {
            QMessageBox::critical(this, "Error", 
                "Failed to open file: " + handler.errorString());
            return;
        }
        loadCpackFromHandler(handler, path, true);
    } else {
        openPlainFileWithPrompt(path);
    }
}

void MainWindow::saveFile() {
    if (currentFilePath_.isEmpty()) {
        saveFileAs();
        return;
    }
    
    CpackFileHandler handler;
    
    syncEditorToMode();

    handler.addFile("soluiton.cpp", currentSolutionCode_.toUtf8());
    handler.addFile("brute.cpp", currentBruteCode_.toUtf8());
    handler.addFile("generator.cpp", currentGeneratorCode_.toUtf8());
    handler.addFile("template.cpp", currentTemplate_.toUtf8());

    if (problemEdited_) {
        handler.addFile("problem.json", currentProblemRaw_.toUtf8());
    } else if (!currentProblem_.isEmpty()) {
        QJsonDocument problemDoc(currentProblem_);
        handler.addFile("problem.json", problemDoc.toJson(QJsonDocument::Indented));
    }

    if (testcasesEdited_) {
        handler.addFile("testcases.json", currentTestcasesRaw_.toUtf8());
    } else {
        QJsonArray testsArray;
        for (const auto &widgets : caseWidgets_) {
            QJsonObject testObj;
            if (widgets.inputEditor) {
                testObj["input"] = widgets.inputEditor->toPlainText();
            }
            if (widgets.expectedEditor) {
                testObj["output"] = widgets.expectedEditor->toPlainText();
            }
            testsArray.append(testObj);
        }
        if (!testsArray.isEmpty()) {
            QJsonObject testsDoc;
            testsDoc["tests"] = testsArray;
            testsDoc["timeout"] = currentTimeout_;
            QJsonDocument doc(testsDoc);
            handler.addFile("testcases.json", doc.toJson(QJsonDocument::Indented));
        }
    }
    
    if (!handler.save(currentFilePath_)) {
        QMessageBox::critical(this, "Error", 
            "Failed to save file: " + handler.errorString());
    } else {
        hasSavedFile_ = true;
        setDirty(false);
        if (currentFilePath_.endsWith(".cpack", Qt::CaseInsensitive)) {
            setPlainTextMode(false);
        }
    }
}

void MainWindow::saveFileAs() {
    saveFileAsWithTitle("Save Problem");
}

void MainWindow::saveFileAsWithTitle(const QString &title) {
    QString path = QFileDialog::getSaveFileName(
        this,
        title,
        currentFilePath_.isEmpty() ? "problem.cpack" : currentFilePath_,
        "cpack Files (*.cpack)"
    );

    if (path.isEmpty()) {
        return;
    }

    if (!path.endsWith(".cpack", Qt::CaseInsensitive)) {
        path += ".cpack";
    }
    currentFilePath_ = path;
    saveFile();
}

void MainWindow::openPlainFileWithPrompt(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Error",
            "Failed to open file: " + file.errorString());
        return;
    }

    QFileInfo info(path);
    baseWindowTitle_ = info.fileName().isEmpty()
        ? QString("CF Dojo - v1.0")
        : QString("CF Dojo - %1").arg(info.fileName());

    const QString contents = QString::fromUtf8(file.readAll());
    currentSolutionCode_ = contents;
    if (codeEditor_) {
        DirtyScope guard(this);
        codeEditor_->setText(contents);
    }

    editorMode_ = EditorMode::Solution;
    updateEditorModeButtons();
    updateWindowTitle();
    currentFilePath_.clear();
    currentTemplate_ = loadDefaultTemplate();
    currentProblem_ = QJsonObject();
    currentProblemRaw_.clear();
    currentTestcasesRaw_.clear();
    problemEdited_ = false;
    testcasesEdited_ = false;
    currentTimeout_ = 5;
    updateProblemMetaUi();
    hasSavedFile_ = false;
    setDirty(false);
    populateCpackTree();
    updateTemplateAvailability();
    updateEditorModeButtons();

    QMessageBox box(this);
    box.setWindowTitle("Convert to CPack?");
    box.setIcon(QMessageBox::Question);
    box.setText("This is a plain text file.");
    box.setInformativeText(
        "Convert to .cpack to enable templates, tests, and problem data.\n"
        "You can also convert later from the banner.");
    auto *saveBtn = box.addButton("Convert to CPack", QMessageBox::AcceptRole);
    box.addButton("Keep as Text", QMessageBox::RejectRole);
    box.setDefaultButton(saveBtn);
    box.exec();

    if (box.clickedButton() == saveBtn) {
        saveFileAsWithTitle("Save as CPack");
        if (!hasSavedFile_) {
            setPlainTextMode(true);
            return;
        }
        setPlainTextMode(false);
        return;
    }
    setPlainTextMode(true);
}

void MainWindow::setPlainTextMode(bool enabled) {
    plainTextMode_ = enabled;
    const QColor normal = themeManager_.textColor();
    const QColor inactive = QColor("#808080");
    const QColor edge = themeManager_.edgeColor();

    auto applyDefaultTint = [normal, inactive](ActivityBarButton *button) {
        if (!button) {
            return;
        }
        button->setTintColors(normal, normal, inactive);
    };

    auto applyDisabledTint = [edge](ActivityBarButton *button) {
        if (!button) {
            return;
        }
        button->setTintColors(edge, edge, edge);
    };

    auto setMenuButtonState = [this, normal, edge](QPushButton *button,
                                                   const QString &iconPath,
                                                   bool isEnabled) {
        if (!button) {
            return;
        }
        const QColor tint = isEnabled ? normal : edge;
        button->setEnabled(isEnabled);
        button->setIcon(IconUtils::makeTintedIcon(iconPath, tint, QSize(16, 16)));
    };

    if (enabled) {
        if (plainTextBanner_) {
            plainTextBanner_->setVisible(true);
        }
        if (mainSplitter_) {
            wasSidePanelCollapsed_ = mainSplitter_->isCollapsed();
            if (!wasSidePanelCollapsed_) {
                mainSplitter_->collapse();
            }
        }
        if (sidebarToggle_) {
            sidebarToggle_->setActiveState(false);
            sidebarToggle_->setChecked(false);
            sidebarToggle_->setEnabled(false);
            applyDisabledTint(sidebarToggle_);
        }
        if (stressTestButton_) {
            stressTestButton_->setActiveState(false);
            stressTestButton_->setChecked(false);
            stressTestButton_->setEnabled(false);
            applyDisabledTint(stressTestButton_);
        }
        if (templateButton_) {
            templateButton_->setActiveState(false);
            templateButton_->setChecked(false);
            templateButton_->setEnabled(false);
            applyDisabledTint(templateButton_);
        }
        setMenuButtonState(menuRunAllButton_, ":/images/play.svg", false);
        setMenuButtonState(menuTemplateButton_, ":/images/template.svg", false);
        return;
    }

    if (plainTextBanner_) {
        plainTextBanner_->setVisible(false);
    }
    if (sidebarToggle_) {
        sidebarToggle_->setEnabled(true);
        applyDefaultTint(sidebarToggle_);
    }
    if (stressTestButton_) {
        stressTestButton_->setEnabled(true);
        applyDefaultTint(stressTestButton_);
    }
    if (templateButton_) {
        templateButton_->setEnabled(true);
        applyDefaultTint(templateButton_);
    }
    setMenuButtonState(menuRunAllButton_, ":/images/play.svg", true);
    setMenuButtonState(menuTemplateButton_, ":/images/template.svg", true);
    if (mainSplitter_ && !wasSidePanelCollapsed_ && mainSplitter_->isCollapsed()) {
        mainSplitter_->expand();
    }
    updateActivityBarActiveStates(mainSplitter_ && mainSplitter_->isCollapsed());
}

void MainWindow::setupCompanionListener() {
    companionListener_ = new CompanionListener(this);
    
    connect(companionListener_, &CompanionListener::problemReceived,
            this, &MainWindow::onProblemReceived);
    
    connect(companionListener_, &CompanionListener::errorOccurred,
            this, [](const QString &error) {
        qWarning() << "Companion listener error:" << error;
    });
    
    if (!companionListener_->start()) {
        qWarning() << "Failed to start Competitive Companion listener on any port";
    } else {
        qDebug() << "Competitive Companion listener started on port" << companionListener_->port();
    }
}

void MainWindow::onProblemReceived(const QJsonObject &problem) {
    if (!confirmDiscardUnsaved("importing a new problem")) {
        return;
    }
    qDebug() << "Problem received:" << problem["name"].toString();
    
    // Ensure UI is ready
    if (!testPanelWidgets_.casesLayout || !testPanelWidgets_.casesContainer) {
        qWarning() << "Test panel not initialized yet";
        return;
    }
    
    // Generate filename from problem name (sanitize for filesystem)
    QString problemName = problem["name"].toString();
    QString filename = problemName;
    filename.replace(QRegularExpression("[^a-zA-Z0-9_\\- ]"), "");
    filename.replace(" ", "_");
    if (filename.isEmpty()) {
        filename = "problem";
    }
    
    QString cpackPath = QDir::current().filePath(filename + ".cpack");
    qDebug() << "Looking for cpack at:" << cpackPath;
    qDebug() << "File exists:" << QFile::exists(cpackPath);
    
    // Check if .cpack file already exists
    if (QFile::exists(cpackPath)) {
        CpackFileHandler handler;
        if (handler.load(cpackPath)) {
            loadCpackFromHandler(handler, cpackPath, true);
            baseWindowTitle_ = QString("CF Dojo - %1").arg(problemName);
            setDirty(false);
            raise();
            activateWindow();
            return;
        }
    }
    
    // No existing file - prompt user
    QMessageBox::StandardButton reply = QMessageBox::question(
        this,
        "New Problem",
        QString("Problem \"%1\" received from Competitive Companion.\n\n"
                "No existing file found. Create a new problem?").arg(problemName),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::Yes
    );
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // Store problem metadata
    currentProblem_ = problem;
    currentProblemRaw_ = QString::fromUtf8(
        QJsonDocument(problem).toJson(QJsonDocument::Indented));
    problemEdited_ = false;
    
    // Clear existing content
    DirtyScope guard(this);
    if (codeEditor_) {
        codeEditor_->clear();
    }
    currentSolutionCode_.clear();
    currentBruteCode_.clear();
    currentGeneratorCode_.clear();
    editorMode_ = EditorMode::Solution;
    updateEditorModeButtons();
    currentTemplate_ = loadDefaultTemplate();
    currentTestcasesRaw_.clear();
    testcasesEdited_ = false;
    currentTimeout_ = 5;
    updateProblemMetaUi();
    
    // Clear existing test cases and add new ones from problem
    clearAllTestCases();
    
    QJsonArray tests = problem["tests"].toArray();
    for (const QJsonValue &testVal : tests) {
        QJsonObject test = testVal.toObject();
        QString input = test["input"].toString();
        QString output = test["output"].toString();
        
        // Add test case
        addTestCase();
        
        // Fill in the test case data
        if (!caseWidgets_.empty()) {
            auto &widgets = caseWidgets_.back();
            if (widgets.inputEditor) {
                widgets.inputEditor->setPlainText(input);
            }
            if (widgets.expectedEditor) {
                widgets.expectedEditor->setPlainText(output);
            }
        }
    }
    
    // If no tests, add one empty test case
    if (tests.isEmpty()) {
        addTestCase();
    }
    currentTestcasesRaw_ = buildTestcasesJson();

    updateTemplateAvailability();
    updateEditorModeButtons();
    
    // Update window title with problem name
    if (!problemName.isEmpty()) {
        baseWindowTitle_ = QString("CF Dojo - %1").arg(problemName);
    }
    
    // Set as default save path (in current directory)
    currentFilePath_ = cpackPath;
    hasSavedFile_ = false;
    setDirty(false);
    
    // Bring window to front
    raise();
    activateWindow();
}
