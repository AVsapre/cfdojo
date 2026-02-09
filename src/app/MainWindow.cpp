#include "app/MainWindow.h"
#include "app/ActivityBarButton.h"
#include "app/CollapsibleSplitter.h"
#include "app/SettingsDialog.h"
#include "companion/CompanionListener.h"
#include "execution/CompilationUtils.h"
#include "file/CpackFileHandler.h"
#include "ui/AutoResizingTextEdit.h"
#include "ui/FileExplorerBuilder.h"
#include "ui/IconUtils.h"
#include "ui/StressPanelBuilder.h"
#include "Version.h"

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
#include <optional>

namespace {
constexpr int kActivityBarWidth = 50;
constexpr int kSidePanelDefaultWidth = 240;
constexpr int kSidePanelMinWidth = 175;

QString loadDefaultTemplate(const QString &language) {
    QSettings settings("CF Dojo", "CF Dojo");
    const QString key = QStringLiteral("defaultTemplate/%1").arg(language);
    const QString stored = settings.value(key).toString();
    return stored.isEmpty() ? QString{CompilationUtils::kDefaultTemplateCode} : stored;
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
    loadRuntimeSettings();
    currentTemplate_ = defaultTemplates_.value(
        CompilationUtils::normalizeLanguage(defaultLanguage_),
        QString{CompilationUtils::kDefaultTemplateCode});
    applyRuntimeSettings();
    themeManager_.apply(qApp, uiScale_);
    executionController_->setIconTintColor(themeManager_.textColor());
    executionController_->setStatusColors(themeManager_.colors().statusAc,
                                          themeManager_.colors().statusError);
    setupUi();
    setupZoomShortcuts();
    setupCompanionListener();
    applyUiZoom();
    setupAutosave();

    // Restore window geometry from previous session
    {
        QSettings settings("CF Dojo", "CF Dojo");
        const QByteArray geo = settings.value("windowGeometry").toByteArray();
        const QByteArray state = settings.value("windowState").toByteArray();
        if (!geo.isEmpty()) restoreGeometry(geo);
        if (!state.isEmpty()) restoreState(state);
    }
    
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

MainWindow::~MainWindow() {
    // Ensure any in-flight stress test future finishes before members are
    // destroyed.  Without this, QtConcurrent may dereference a dangling
    // pointer to this MainWindow.
    if (stressWatcher_) {
        stressWatcher_->cancel();
        stressWatcher_->waitForFinished();
    }
}

void MainWindow::loadRuntimeSettings() {
    QSettings settings("CF Dojo", "CF Dojo");
    defaultTranscludeTemplateEnabled_ =
        settings.value("transcludeTemplate", false).toBool();
    transcludeTemplateEnabled_ = defaultTranscludeTemplateEnabled_;

    const int autosaveSec = std::clamp(
        settings.value("autosaveIntervalSec", 15).toInt(), 5, 300);
    autosaveIntervalMs_ = autosaveSec * 1000;

    defaultLanguage_ = CompilationUtils::normalizeLanguage(
        settings.value("defaultLanguage", "C++").toString());
    currentLanguage_ = defaultLanguage_;
    compilationConfig_.cppCompilerPath = settings.value("cppCompilerPath", "g++").toString();
    compilationConfig_.cppCompilerFlags = settings.value("cppCompilerFlags", "-O2 -std=c++17").toString();
    compilationConfig_.pythonPath = settings.value("pythonPath", "python3").toString();
    compilationConfig_.pythonArgs = settings.value("pythonArgs", "").toString();
    compilationConfig_.javaCompilerPath = settings.value("javaCompilerPath", "javac").toString();
    compilationConfig_.javaRunPath = settings.value("javaRunPath", "java").toString();
    compilationConfig_.javaArgs = settings.value("javaArgs", "").toString();

    for (const QString &lang : CompilationUtils::supportedLanguages()) {
        defaultTemplates_[lang] = loadDefaultTemplate(lang);
    }

    fileExplorerRootDir_ = settings.value("rootDir", QDir::currentPath()).toString();

    uiScale_ = settings.value("uiScale", 1.0).toDouble();
    uiScale_ = std::clamp(uiScale_, 0.7, 1.8);
    if (fileExplorerRootDir_.trimmed().isEmpty() ||
        !QFileInfo::exists(fileExplorerRootDir_) ||
        !QFileInfo(fileExplorerRootDir_).isDir()) {
        fileExplorerRootDir_ = QDir::currentPath();
    }
}

void MainWindow::applyRuntimeSettings() {
    compilationConfig_.language = CompilationUtils::normalizeLanguage(currentLanguage_);
    compilationConfig_.transcludeTemplate = transcludeTemplateEnabled_;
    compilationConfig_.templateCode = currentTemplate_;
    if (executionController_) {
        executionController_->setConfig(compilationConfig_);
    }
    if (parallelExecutor_) {
        parallelExecutor_->setConfig(compilationConfig_);
    }
}

void MainWindow::applyFileExplorerRootDirectory(const QString &path) {
    if (!fileModel_ || !fileTree_) {
        return;
    }
    if (path.trimmed().isEmpty()) {
        return;
    }
    if (!QFileInfo::exists(path) || !QFileInfo(path).isDir()) {
        return;
    }

    fileModel_->setRootPath(path);
    fileTree_->setRootIndex(fileModel_->index(path));
}

QString MainWindow::languageForPath(const QString &path) const {
    const QString ext = QFileInfo(path).suffix().trimmed().toLower();
    if (ext == "py") {
        return "Python";
    }
    if (ext == "java") {
        return "Java";
    }
    if (ext == "cpp" || ext == "cc" || ext == "cxx" ||
        ext == "c++" || ext == "h" || ext == "hpp" ||
        ext == "hh" || ext == "hxx") {
        return "C++";
    }
    return defaultLanguage_;
}

void MainWindow::setCurrentLanguage(const QString &language) {
    currentLanguage_ = CompilationUtils::normalizeLanguage(language);
    applyRuntimeSettings();
}

void MainWindow::setBaseWindowTitle(const QString &title) {
    baseWindowTitle_ = title;
    updateWindowTitle();
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

    // Persist window geometry so it reopens in the same place
    QSettings settings("CF Dojo", "CF Dojo");
    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("uiScale", uiScale_);

    QMainWindow::closeEvent(event);
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
    baseWindowTitle_ = QStringLiteral("CF Dojo");
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

    // Helper: switch side panel to the given widget and update check states
    auto switchPanel = [this](QWidget *panel) {
        if (!mainSplitter_ || !sideStack_ || !panel) return;
        sideStack_->setCurrentWidget(panel);
        if (mainSplitter_->isCollapsed()) mainSplitter_->expand();
        for (auto *btn : {sidebarToggle_, stressTestButton_, templateButton_, newFileButton_}) {
            if (btn) btn->setChecked(false);
        }
        // The caller's button gets re-checked via updateActivityBarActiveStates below
    };

    connect(sidebarToggle_, &QPushButton::clicked, this, [this, switchPanel]() {
        if (!mainSplitter_ || !sideStack_) return;
        // If already showing test panel and not collapsed, collapse
        if (!mainSplitter_->isCollapsed() &&
            sideStack_->currentWidget() == testPanelWidgets_.panel) {
            mainSplitter_->collapse();
            sidebarToggle_->setChecked(false);
            return;
        }
        switchPanel(testPanelWidgets_.panel);
        sidebarToggle_->setChecked(true);
    });

    // Stress test button
    stressTestButton_ = new ActivityBarButton(":/images/stresstest.svg", activityBar_);
    stressTestButton_->setObjectName("StressTestButton");
    stressTestButton_->setFixedHeight(kActivityBarWidth);
    stressTestButton_->setToolTip("Stress Test");
    stressTestButton_->setCheckable(true);
    stressTestButton_->setChecked(false);
    applyActivityTint(stressTestButton_);

    connect(stressTestButton_, &QPushButton::clicked, this, [this, switchPanel]() {
        switchPanel(stressTestPanel_);
        if (stressTestButton_) stressTestButton_->setChecked(true);
    });

    // Template editor button
    templateButton_ = new ActivityBarButton(":/images/template.svg", activityBar_);
    templateButton_->setObjectName("TemplateButton");
    templateButton_->setFixedHeight(kActivityBarWidth);
    templateButton_->setToolTip("Show template view");
    templateButton_->setCheckable(true);
    templateButton_->setChecked(false);
    applyActivityTint(templateButton_);

    connect(templateButton_, &QPushButton::clicked, this, [this, switchPanel]() {
        switchPanel(cpackPanel_);
        if (templateButton_) templateButton_->setChecked(true);
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

    connect(newFileButton_, &QPushButton::clicked, this, [this, switchPanel]() {
        switchPanel(fileExplorer_);
        if (newFileButton_) newFileButton_->setChecked(true);
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
}

void MainWindow::openSettingsDialog() {
    auto populateSettings = [this]() {
        for (const QString &lang : CompilationUtils::supportedLanguages()) {
            settingsWindow_->setTemplateForLanguage(
                lang, defaultTemplates_.value(
                    lang, QString{CompilationUtils::kDefaultTemplateCode}));
        }
        settingsWindow_->setMultithreadingEnabled(multithreadingEnabled_);
        settingsWindow_->setTranscludeTemplateEnabled(defaultTranscludeTemplateEnabled_);
        settingsWindow_->setAutosaveIntervalSeconds(autosaveIntervalMs_ / 1000);
        settingsWindow_->setDefaultLanguage(defaultLanguage_);
        settingsWindow_->setCompilerPath(compilationConfig_.cppCompilerPath);
        settingsWindow_->setCompilerFlags(compilationConfig_.cppCompilerFlags);
        settingsWindow_->setPythonPath(compilationConfig_.pythonPath);
        settingsWindow_->setPythonArgs(compilationConfig_.pythonArgs);
        settingsWindow_->setJavaCompilerPath(compilationConfig_.javaCompilerPath);
        settingsWindow_->setJavaRunPath(compilationConfig_.javaRunPath);
        settingsWindow_->setJavaArgs(compilationConfig_.javaArgs);
        settingsWindow_->setRootDir(fileExplorerRootDir_);
    };

    if (!settingsWindow_) {
        settingsWindow_ = new SettingsDialog(this);
        populateSettings();

        // Note: We do NOT connect settingsChanged to markDirty here.
        // The settings dialog marks things as changed on any widget interaction,
        // which would incorrectly dirty the document. Only the 'saved' signal
        // should actually update state.

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
            QSettings settings("CF Dojo", "CF Dojo");
            for (const QString &lang : CompilationUtils::supportedLanguages()) {
                defaultTemplates_[lang] = settingsWindow_->getTemplateForLanguage(lang);
                settings.setValue(QStringLiteral("defaultTemplate/%1").arg(lang),
                                  defaultTemplates_[lang]);
            }
            currentTemplate_ = defaultTemplates_.value(
                CompilationUtils::normalizeLanguage(currentLanguage_),
                QString{CompilationUtils::kDefaultTemplateCode});
            multithreadingEnabled_ = settingsWindow_->isMultithreadingEnabled();
            defaultTranscludeTemplateEnabled_ =
                settingsWindow_->isTranscludeTemplateEnabled();
            settings.setValue("transcludeTemplate", defaultTranscludeTemplateEnabled_);
            const QString previousDefaultLanguage = CompilationUtils::normalizeLanguage(defaultLanguage_);
            defaultLanguage_ = CompilationUtils::normalizeLanguage(settingsWindow_->defaultLanguage());
            if (CompilationUtils::normalizeLanguage(currentLanguage_) == previousDefaultLanguage) {
                currentLanguage_ = defaultLanguage_;
            }
            compilationConfig_.cppCompilerPath = settingsWindow_->compilerPath();
            compilationConfig_.cppCompilerFlags = settingsWindow_->compilerFlags();
            compilationConfig_.pythonPath = settingsWindow_->pythonPath();
            compilationConfig_.pythonArgs = settingsWindow_->pythonArgs();
            compilationConfig_.javaCompilerPath = settingsWindow_->javaCompilerPath();
            compilationConfig_.javaRunPath = settingsWindow_->javaRunPath();
            compilationConfig_.javaArgs = settingsWindow_->javaArgs();
            fileExplorerRootDir_ = settingsWindow_->rootDir().trimmed();
            if (fileExplorerRootDir_.isEmpty() ||
                !QFileInfo::exists(fileExplorerRootDir_) ||
                !QFileInfo(fileExplorerRootDir_).isDir()) {
                fileExplorerRootDir_ = QDir::currentPath();
            }
            settings.setValue("rootDir", fileExplorerRootDir_);
            applyRuntimeSettings();
            updateTemplateAvailability();
            applyFileExplorerRootDirectory(fileExplorerRootDir_);
            settings.setValue("autosaveIntervalSec", settingsWindow_->autosaveIntervalSeconds());
            autosaveIntervalMs_ = settingsWindow_->autosaveIntervalSeconds() * 1000;
            if (isDirty_) {
                scheduleAutosave();
            }
            settings.setValue("defaultLanguage", defaultLanguage_);
            settings.setValue("cppCompilerPath", compilationConfig_.cppCompilerPath);
            settings.setValue("cppCompilerFlags", compilationConfig_.cppCompilerFlags);
            settings.setValue("pythonPath", compilationConfig_.pythonPath);
            settings.setValue("pythonArgs", compilationConfig_.pythonArgs);
            settings.setValue("javaCompilerPath", compilationConfig_.javaCompilerPath);
            settings.setValue("javaRunPath", compilationConfig_.javaRunPath);
            settings.setValue("javaArgs", compilationConfig_.javaArgs);
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
        populateSettings();
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
        const QString rootPath = fileExplorerRootDir_.isEmpty()
            ? QDir::currentPath()
            : fileExplorerRootDir_;
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
        const QString filePath = fileModel_->filePath(index);
        if (filePath.endsWith(".cpack", Qt::CaseInsensitive)) {
            // Validate the file first, before asking to discard
            CpackFileHandler handler;
            if (!handler.load(filePath)) {
                QMessageBox::critical(this, "Error",
                    "Failed to open file: " + handler.errorString());
                return;
            }
            if (!confirmDiscardUnsaved("opening another file")) {
                return;
            }
            loadCpackFromHandler(handler, filePath, true);
        }
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
        if (target && target->metaObject()->indexOfSlot(
                QMetaObject::normalizedSignature(QByteArray(slot) + "()")) != -1) {
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
        "<li>Create a new problem or open a .cpack/import from Competitive Companion.</li>"
        "<li>Write your solution in solution.cpp.</li>"
        "<li>Add test cases and click Run All.</li>"
        "</ol>"
        "<h3>New / Open / Save</h3>"
        "<ul>"
        "<li>New asks for a language (C++/Java/Python), then prompts for a .cpack path.</li>"
        "<li>Open supports .cpack and plain text files, with language auto-detection for common extensions.</li>"
        "<li>Plain text mode has limited features; convert via the banner.</li>"
        "</ul>"
        "<h3>Testing & stress</h3>"
        "<ul>"
        "<li>Run All executes your local test cases.</li>"
        "<li>Stress testing uses generator.cpp + brute.cpp.</li>"
        "</ul>"
        "<h3>Templates</h3>"
        "<p>template.cpp uses //#main to insert your solution. "
        "Template view default in Settings applies to new/opened files, not the current one.</p>"
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
        QString("<h2>CF Dojo %1</h2>").arg(CFDojo::versionString()) +
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
        "<li>Settings - Alzam - <a href=\"https://thenounproject.com/icon/5079171/\">source</a></li>"
        "<li>Attack (app icon) - Good Wife - <a href=\"https://thenounproject.com/icon/attack-5572740/\">source</a></li>"
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

void MainWindow::showBottomToast(const QString &message) {
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

    copyToastLabel_->setText(message);
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

void MainWindow::showCopyToast() {
    showBottomToast("Answer copied to clipboard.");
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
    // Determine file extension from current language
    const QString lang = CompilationUtils::normalizeLanguage(currentLanguage_);
    const QString ext = (lang == "Python") ? "py"
                      : (lang == "Java")   ? "java"
                                           : "cpp";

    QString modeLabel;
    switch (editorMode_) {
    case EditorMode::Solution:
        modeLabel = QString("solution.%1").arg(ext);
        break;
    case EditorMode::Brute:
        modeLabel = QString("brute.%1").arg(ext);
        break;
    case EditorMode::Generator:
        modeLabel = QString("generator.%1").arg(ext);
        break;
    case EditorMode::Template:
        modeLabel = QString("template.%1").arg(ext);
        break;
    case EditorMode::Problem:
        modeLabel = "problem.json";
        break;
    case EditorMode::Testcases:
        modeLabel = "testcases.json";
        break;
    }
    QString editLabel;
    if (hasSavedFile_ && !currentFilePath_.isEmpty()) {
        editLabel = QFileInfo(currentFilePath_).completeBaseName();
    } else {
        editLabel = "Untitled";
    }
    const QString dirtyPrefix = isDirty_ ? "* " : "";
    setWindowTitle(QString("%1%2 \u2013 %3 \u2014 %4")
                       .arg(dirtyPrefix, modeLabel, editLabel, baseWindowTitle_));
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

void MainWindow::setupAutosave() {
    if (!autosaveTimer_) {
        autosaveTimer_ = new QTimer(this);
        autosaveTimer_->setSingleShot(true);
        connect(autosaveTimer_, &QTimer::timeout, this, &MainWindow::performAutosave);
    }
}

void MainWindow::scheduleAutosave() {
    if (!autosaveTimer_) {
        return;
    }
    autosaveTimer_->start(std::max(1000, autosaveIntervalMs_));
}

void MainWindow::performAutosave() {
    if (!isDirty_) {
        return;
    }

    syncEditorToMode();
    CpackFileHandler handler;
    handler.addFile("solution.cpp", currentSolutionCode_.toUtf8());
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


void MainWindow::loadCpackFromHandler(const CpackFileHandler &handler,
                                      const QString &path,
                                      bool markSavedFile) {
    DirtyScope guard(this);
    setCurrentLanguage(defaultLanguage_);
    transcludeTemplateEnabled_ = defaultTranscludeTemplateEnabled_;
    applyRuntimeSettings();
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

    // Require canonical solution filename
    if (!handler.hasFile("solution.cpp")) {
        QMessageBox::critical(this, "Error", "Unsupported CPack format");
        return;
    }

    currentSolutionCode_.clear();
    currentSolutionCode_ = QString::fromUtf8(handler.getFile("solution.cpp"));
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
        currentTemplate_ = defaultTemplates_.value(
            CompilationUtils::normalizeLanguage(currentLanguage_),
            QString{CompilationUtils::kDefaultTemplateCode});
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
        baseWindowTitle_ = QStringLiteral("CF Dojo");
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
    // Map each button to its associated side-panel widget (nullptr = test panel / default)
    const std::initializer_list<std::pair<ActivityBarButton *, QWidget *>> mapping = {
        {sidebarToggle_,    nullptr},           // test panel (default)
        {newFileButton_,    fileExplorer_},
        {stressTestButton_, stressTestPanel_},
        {templateButton_,   cpackPanel_},
    };

    QWidget *currentPanel = (!collapsed && sideStack_) ? sideStack_->currentWidget() : nullptr;

    for (auto &[button, panel] : mapping) {
        if (!button) continue;
        // A button is active when not collapsed and its panel is showing.
        // sidebarToggle_ (panel==nullptr) is active when the test panel is showing
        // (i.e. none of the other panels matched).
        const bool active = !collapsed &&
            (panel ? (currentPanel == panel)
                   : (currentPanel != nullptr &&
                      currentPanel != fileExplorer_ &&
                      currentPanel != stressTestPanel_ &&
                      currentPanel != cpackPanel_));
        button->setActiveState(active);
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

    // Update template button tooltip
    if (templateButton_) {
        templateButton_->setToolTip(transcludeTemplateEnabled_
            ? "Hide template"
            : "Show template");
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
            applyRuntimeSettings();
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

        applyRuntimeSettings();
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
            widgets.statusLabel->setStyleSheet(
                QString("color: %1; font-weight: 700;")
                    .arg(themeManager_.colors().statusError.name()));
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
            stressStatusLabel_->setStyleSheet(QString("color: %1; font-weight: 700;").arg(themeManager_.colors().statusError.name()));
        }
        if (stressComplexityLabel_) {
            stressComplexityLabel_->setText("Suspected: insufficient timing data");
            stressComplexityLabel_->setVisible(true);
        }
        if (stressLog_) {
            stressLog_->setPlainText(
                "Please provide solution, brute, and generator code before running stress test.");
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
                    stressStatusLabel_->setStyleSheet(QString("color: %1; font-weight: 700;").arg(themeManager_.colors().statusError.name()));
                }
                if (stressLog_) {
                    stressLog_->setPlainText(result.error);
                }
                return;
            }

            if (result.passed) {
                if (stressStatusLabel_) {
                    stressStatusLabel_->setText("Passed");
                    stressStatusLabel_->setStyleSheet(QString("color: %1; font-weight: 700;").arg(themeManager_.colors().statusAc.name()));
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
                stressStatusLabel_->setStyleSheet(QString("color: %1; font-weight: 700;").arg(themeManager_.colors().statusError.name()));
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
    CompilationConfig configCopy = compilationConfig_;
    configCopy.templateCode = tmpl;
    configCopy.transcludeTemplate = transcludeTemplateEnabled_;

    const bool useParallel = multithreadingEnabled_;
    stressWatcher_->setFuture(QtConcurrent::run([this, count, solutionCopy, bruteCopy, generatorCopy, configCopy, timeoutMs, useParallel]() {
        return runStressTestWorker(count, solutionCopy, bruteCopy, generatorCopy, configCopy,
                                   timeoutMs, useParallel);
    }));
}

MainWindow::StressResult MainWindow::runStressTestWorker(int count,
                                                        const QString &solution,
                                                        const QString &brute,
                                                        const QString &generator,
                                                        const CompilationConfig &config,
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

    const QString tempPath = tempDir.path();
    const QString language = CompilationUtils::normalizeLanguage(config.language);

#ifdef Q_OS_WIN
    const QString exeSuffix = ".exe";
#else
    const QString exeSuffix;
#endif

    // ── Per-source preparation ──────────────────────────────────────────
    // For each of the 3 sources (generator, brute, solution) we need:
    //   runProgram  – the executable / interpreter path
    //   runArgs     – arguments to pass before piping stdin
    // C++:    compile → run the native binary
    // Python: no compile → run via interpreter
    // Java:   compile with javac → run via java -cp

    struct SourceBinary {
        QString program;
        QStringList args;
    };

    auto prepareSource = [&](const QString &rawCode,
                             const QString &label,
                             const QString &baseName,
                             QString *errorOut) -> std::optional<SourceBinary> {
        const QString code = CompilationUtils::applyTransclusion(
            config.templateCode, rawCode, config.transcludeTemplate);

        const QString sourceExt = language == "Python" ? "py"
                                : language == "Java"   ? "java"
                                                       : "cpp";

        // Each source gets its own subdirectory to avoid name collisions
        // (e.g. Java files with the same public class name).
        const QString sourceDir = QDir(tempPath).filePath(baseName);
        QDir().mkpath(sourceDir);

        // For Java, detect the public class name from the *transcluded* code
        const QString sourceBaseName =
            language == "Java" ? CompilationUtils::detectJavaMainClass(code) : baseName;
        const QString sourcePath =
            QDir(sourceDir).filePath(QString("%1.%2").arg(sourceBaseName, sourceExt));

        // Write the source file
        QFile file(sourcePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            if (errorOut) *errorOut = QString("Failed to write %1 source").arg(label);
            return std::nullopt;
        }
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << code;
        file.close();

        SourceBinary bin;

        if (language == "Python") {
            // No compilation needed — run through interpreter
#ifdef Q_OS_WIN
            const QString defaultPython = "python";
#else
            const QString defaultPython = "python3";
#endif
            bin.program = config.pythonPath.trimmed().isEmpty()
                ? defaultPython : config.pythonPath.trimmed();
            bin.args = CompilationUtils::splitArgs(config.pythonArgs);
            bin.args << sourcePath;
            return bin;
        }

        if (language == "Java") {
            // Compile with javac
            const QString javacPath = config.javaCompilerPath.trimmed().isEmpty()
                ? "javac" : config.javaCompilerPath.trimmed();
            QProcess compiler;
            compiler.setWorkingDirectory(sourceDir);
            compiler.start(javacPath, {sourcePath});
            if (!compiler.waitForFinished(30000)) {
                if (errorOut) *errorOut = QString("%1 compilation timed out").arg(label);
                return std::nullopt;
            }
            if (compiler.exitCode() != 0) {
                const QString err = QString::fromUtf8(compiler.readAllStandardError());
                if (errorOut) *errorOut = err.isEmpty()
                    ? QString("%1 compilation failed").arg(label) : err;
                return std::nullopt;
            }
            bin.program = config.javaRunPath.trimmed().isEmpty()
                ? "java" : config.javaRunPath.trimmed();
            bin.args = CompilationUtils::splitArgs(config.javaArgs);
            bin.args << "-cp" << sourceDir << sourceBaseName;
            return bin;
        }

        // C++ — compile to native binary
        const QString exePath = QDir(sourceDir).filePath(baseName + exeSuffix);
        QStringList compileArgs = CompilationUtils::splitArgs(config.cppCompilerFlags);
        if (compileArgs.isEmpty()) {
            compileArgs << "-O2" << "-std=c++17";
        }
        compileArgs << sourcePath << "-o" << exePath;

        const QString compilerPath = config.cppCompilerPath.trimmed().isEmpty()
            ? "g++" : config.cppCompilerPath.trimmed();
        QProcess compiler;
        compiler.setWorkingDirectory(tempPath);
        compiler.start(compilerPath, compileArgs);
        if (!compiler.waitForFinished(30000)) {
            if (errorOut) *errorOut = QString("%1 compilation timed out").arg(label);
            return std::nullopt;
        }
        if (compiler.exitCode() != 0) {
            const QString err = QString::fromUtf8(compiler.readAllStandardError());
            if (errorOut) *errorOut = err.isEmpty()
                ? QString("%1 compilation failed").arg(label) : err;
            return std::nullopt;
        }
        bin.program = exePath;
        return bin;
    };

    // Prepare all three sources
    QString prepError;
    auto generatorBin = prepareSource(generator, "Generator", "generator", &prepError);
    if (!generatorBin) {
        result.error = QString("Generator error:\n%1").arg(prepError);
        result.complexity = QString("Suspected: insufficient timing data");
        return result;
    }
    auto bruteBin = prepareSource(brute, "Brute", "brute", &prepError);
    if (!bruteBin) {
        result.error = QString("Brute error:\n%1").arg(prepError);
        result.complexity = QString("Suspected: insufficient timing data");
        return result;
    }
    auto solutionBin = prepareSource(solution, "Solution", "solution", &prepError);
    if (!solutionBin) {
        result.error = QString("Solution error:\n%1").arg(prepError);
        result.complexity = QString("Suspected: insufficient timing data");
        return result;
    }

    auto runProcess = [&](const SourceBinary &bin,
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
        process.start(bin.program, bin.args);
        if (!process.waitForStarted(1000)) {
            if (errorOut) {
                *errorOut = QString("Failed to start %1").arg(bin.program);
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
                *errorOut = QString("Time Limit Exceeded: %1").arg(bin.program);
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
                *errorOut = QString("Runtime Error: %1").arg(bin.program);
            }
            return false;
        }
        return true;
    };

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
        if (!runProcess(*generatorBin, QString(), caseDir, &input, &stderrOut, nullptr, &runError)) {
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
            if (!runProcess(*bruteBin, input, caseDir, &bruteOut, &bruteErr, nullptr, &runError)) {
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
            if (!runProcess(*solutionBin, input, caseDir, &solutionOut, &solutionErr, &solutionTime, &runError)) {
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

            if (CompilationUtils::normalizeText(bruteOut) != CompilationUtils::normalizeText(solutionOut)) {
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
            if (!runProcess(*bruteBin, input, caseDir, &bruteOut, &bruteErr, nullptr, &runError)) {
                caseResult.error = QString("Brute failed on test #%1:\n%2")
                    .arg(index + 1)
                    .arg(runError);
                caseResult.stderrOutput = bruteErr;
                return caseResult;
            }

            QString solutionOut;
            QString solutionErr;
            qint64 solutionTime = -1;
            if (!runProcess(*solutionBin, input, caseDir, &solutionOut, &solutionErr, &solutionTime, &runError)) {
                caseResult.error = QString("Solution failed on test #%1:\n%2")
                    .arg(index + 1)
                    .arg(runError);
                caseResult.stderrOutput = solutionErr;
                return caseResult;
            }

            caseResult.solutionTime = solutionTime;
            if (CompilationUtils::normalizeText(bruteOut) != CompilationUtils::normalizeText(solutionOut)) {
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

void MainWindow::runAllTests() {
    if (caseWidgets_.empty() || !codeEditor_) {
        return;
    }
    applyRuntimeSettings();

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
        if (!result.output.isEmpty()) {
            widgets.outputViewer->setPlainText(result.output);
            widgets.outputViewer->parentWidget()->show();
        } else {
            widgets.outputViewer->parentWidget()->hide();
        }
    }

    if (widgets.errorViewer) {
        if (!result.error.isEmpty()) {
            widgets.errorViewer->setPlainText(result.error);
            widgets.errorViewer->parentWidget()->show();
        } else {
            widgets.errorViewer->parentWidget()->hide();
        }
    }

    if (widgets.outputSplitter) {
        const bool anyVisible =
            (widgets.outputViewer && widgets.outputViewer->parentWidget()->isVisible()) ||
            (widgets.errorViewer && widgets.errorViewer->parentWidget()->isVisible());
        widgets.outputSplitter->setVisible(anyVisible);
    }

        if (widgets.statusLabel) {
            const QString timeSuffix = result.executionTimeMs > 0
                ? QString(" \u2022 %1 ms").arg(result.executionTimeMs)
                : QString();
            const bool isTle = result.error.contains("Time Limit Exceeded");
            if (isTle) {
                widgets.statusLabel->setText("TLE" + timeSuffix);
                widgets.statusLabel->setStyleSheet(QString("color: %1; font-weight: 700;").arg(themeManager_.colors().statusError.name()));
            } else if (result.exitCode != 0 || !result.error.isEmpty()) {
                widgets.statusLabel->setText("Runtime Error" + timeSuffix);
                widgets.statusLabel->setStyleSheet(QString("color: %1; font-weight: 700;").arg(themeManager_.colors().statusError.name()));
            } else if (result.passed) {
                widgets.statusLabel->setText("AC" + timeSuffix);
                widgets.statusLabel->setStyleSheet(QString("color: %1; font-weight: 700;").arg(themeManager_.colors().statusAc.name()));
            } else {
                widgets.statusLabel->setText("Wrong Answer" + timeSuffix);
                widgets.statusLabel->setStyleSheet(QString("color: %1; font-weight: 700;").arg(themeManager_.colors().statusError.name()));
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

    // Scale activity bar and icon buttons
    auto scalePx = [this](int base) -> int {
        return std::max(1, static_cast<int>(std::round(base * uiScale_)));
    };

    if (activityBar_) {
        activityBar_->setFixedWidth(scalePx(kActivityBarWidth));
    }

    auto scaleActivityButton = [&](ActivityBarButton *btn) {
        if (!btn) return;
        btn->setFixedHeight(scalePx(kActivityBarWidth));
        btn->setScale(uiScale_);
    };
    scaleActivityButton(sidebarToggle_);
    scaleActivityButton(stressTestButton_);
    scaleActivityButton(templateButton_);
    scaleActivityButton(newFileButton_);
    scaleActivityButton(settingsButton_);
    scaleActivityButton(backButton_);

    // Scale corner menu icon buttons
    const int iconPx = scalePx(16);
    const QSize iconSz(iconPx, iconPx);
    const QSize btnSz(scalePx(28), scalePx(24));
    auto scaleMenuButton = [&](QPushButton *btn) {
        if (!btn) return;
        btn->setIconSize(iconSz);
        btn->setFixedSize(btnSz);
    };
    scaleMenuButton(menuRunAllButton_);
    // The copy button is found by object name
    if (auto *copyBtn = menuBar_ ? menuBar_->findChild<QPushButton *>("MenuCopyButton") : nullptr) {
        scaleMenuButton(copyBtn);
    }

    // Scale side panel minimum width
    if (sidePanel_) {
        sidePanel_->setMinimumWidth(scalePx(kSidePanelMinWidth));
    }
    if (mainSplitter_) {
        mainSplitter_->setMinimumPanelWidth(scalePx(kSidePanelMinWidth));
        mainSplitter_->setPreferredWidth(scalePx(kSidePanelDefaultWidth));
    }

    // Scale all small icon buttons created by test/stress panel builders.
    // These are found by their Qt object names since they're not stored as
    // MainWindow members.
    const int smallBtnPx = scalePx(28);
    const QSize smallBtnSz(smallBtnPx, smallBtnPx);
    const auto allButtons = findChildren<QPushButton *>();
    for (auto *btn : allButtons) {
        const QString name = btn->objectName();
        if (name == "RunButton" || name == "DeleteButton") {
            btn->setIconSize(iconSz);
            btn->setFixedSize(smallBtnSz);
        } else if (name == "RunAllButton" || name == "ClearCasesButton"
                   || name == "AddCaseButton") {
            btn->setIconSize(iconSz);
            btn->setMinimumHeight(smallBtnPx);
        }
    }

    // Scale the bottom action row height
    const auto actionRows = findChildren<QWidget *>("CasesActionRow");
    for (auto *row : actionRows)
        row->setFixedHeight(smallBtnPx);

    // Scale stress test count field
    const auto countEdits = findChildren<QLineEdit *>("StressGenerateCount");
    for (auto *edit : countEdits)
        edit->setFixedWidth(scalePx(48));

}

void MainWindow::zoomIn() {
    if (uiScale_ < 1.8) {
        uiScale_ = std::min(uiScale_ + 0.1, 1.8);
        applyUiZoom();
        QSettings("CF Dojo", "CF Dojo").setValue("uiScale", uiScale_);
    }
}

void MainWindow::zoomOut() {
    if (uiScale_ > 0.7) {
        uiScale_ = std::max(uiScale_ - 0.1, 0.7);
        applyUiZoom();
        QSettings("CF Dojo", "CF Dojo").setValue("uiScale", uiScale_);
    }
}

void MainWindow::resetZoom() {
    uiScale_ = 1.0;
    applyUiZoom();
    QSettings("CF Dojo", "CF Dojo").setValue("uiScale", uiScale_);
}

void MainWindow::newFile() {
    if (!confirmDiscardUnsaved("creating a new file")) {
        return;
    }

    QString selectedLanguage = defaultLanguage_;
    {
        QDialog picker(this);
        picker.setWindowTitle("New File Language");
        picker.setModal(true);

        auto *layout = new QVBoxLayout(&picker);
        layout->setContentsMargins(16, 16, 16, 16);
        layout->setSpacing(12);

        auto *prompt = new QLabel("Choose language for the new file.", &picker);
        layout->addWidget(prompt);

        auto *buttonRow = new QHBoxLayout();
        buttonRow->setSpacing(8);

        auto *cppBtn = new QPushButton("C++", &picker);
        auto *javaBtn = new QPushButton("Java", &picker);
        auto *pythonBtn = new QPushButton("Python", &picker);
        auto *cancelBtn = new QPushButton("Cancel", &picker);

        buttonRow->addWidget(cppBtn);
        buttonRow->addWidget(javaBtn);
        buttonRow->addWidget(pythonBtn);
        buttonRow->addWidget(cancelBtn);
        layout->addLayout(buttonRow);

        connect(cppBtn, &QPushButton::clicked, &picker, [&picker, &selectedLanguage]() {
            selectedLanguage = "C++";
            picker.accept();
        });
        connect(javaBtn, &QPushButton::clicked, &picker, [&picker, &selectedLanguage]() {
            selectedLanguage = "Java";
            picker.accept();
        });
        connect(pythonBtn, &QPushButton::clicked, &picker, [&picker, &selectedLanguage]() {
            selectedLanguage = "Python";
            picker.accept();
        });
        connect(cancelBtn, &QPushButton::clicked, &picker, &QDialog::reject);

        if (CompilationUtils::normalizeLanguage(defaultLanguage_) == "Python") {
            pythonBtn->setDefault(true);
            pythonBtn->setFocus();
        } else if (CompilationUtils::normalizeLanguage(defaultLanguage_) == "Java") {
            javaBtn->setDefault(true);
            javaBtn->setFocus();
        } else {
            cppBtn->setDefault(true);
            cppBtn->setFocus();
        }

        if (picker.exec() != QDialog::Accepted) {
            return;
        }
    }

    DirtyScope guard(this);
    if (codeEditor_) {
        codeEditor_->clear();
    }
    setCurrentLanguage(selectedLanguage);
    transcludeTemplateEnabled_ = defaultTranscludeTemplateEnabled_;
    applyRuntimeSettings();
    currentFilePath_.clear();
    hasSavedFile_ = false;
    editorMode_ = EditorMode::Solution;
    currentSolutionCode_.clear();
    currentBruteCode_.clear();
    currentGeneratorCode_.clear();
    currentTemplate_ = defaultTemplates_.value(
        CompilationUtils::normalizeLanguage(selectedLanguage),
        QString{CompilationUtils::kDefaultTemplateCode});
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
}

void MainWindow::openFile() {
    // Select file first, before asking to discard unsaved work.
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open CPack File",
        QString(),
        "CPack Files (*.cpack);;All Files (*)"
    );
    
    if (path.isEmpty()) {
        return;
    }
    
    // Validate the file can be loaded before discarding current work.
    CpackFileHandler handler;
    if (!handler.load(path)) {
        QMessageBox::critical(this, "Error", 
            "Failed to open file: " + handler.errorString());
        return;
    }

    // Only now ask the user to discard unsaved work — the new file is known-good.
    if (!confirmDiscardUnsaved("opening another file")) {
        return;
    }

    loadCpackFromHandler(handler, path, true);
}

void MainWindow::saveFile() {
    if (currentFilePath_.isEmpty()) {
        saveFileAs();
        return;
    }
    
    CpackFileHandler handler;
    
    syncEditorToMode();

    handler.addFile("solution.cpp", currentSolutionCode_.toUtf8());
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

void MainWindow::setupCompanionListener() {
    companionListener_ = new CompanionListener(this);
    
    connect(companionListener_, &CompanionListener::problemReceived,
            this, &MainWindow::onProblemReceived);
    
    companionListener_->start();
}

void MainWindow::onProblemReceived(const QJsonObject &problem) {
    if (!confirmDiscardUnsaved("importing a new problem")) {
        return;
    }
    // Ensure UI is ready
    if (!testPanelWidgets_.casesLayout || !testPanelWidgets_.casesContainer) {
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
    
    QString cpackPath = QDir(fileExplorerRootDir_).filePath(filename + ".cpack");
    
    // Check if .cpack file already exists
    if (QFile::exists(cpackPath)) {
        CpackFileHandler handler;
        if (handler.load(cpackPath)) {
            loadCpackFromHandler(handler, cpackPath, true);
            baseWindowTitle_ = QString("CF Dojo - %1").arg(problemName);
            setDirty(false);
            raise();
            activateWindow();
            QApplication::alert(this);
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
    currentTemplate_ = defaultTemplates_.value(
        CompilationUtils::normalizeLanguage(currentLanguage_),
        QString{CompilationUtils::kDefaultTemplateCode});
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
    QApplication::alert(this);
}
