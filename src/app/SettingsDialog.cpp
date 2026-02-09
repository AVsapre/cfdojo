#include "app/SettingsDialog.h"
#include "Version.h"
#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QComboBox>
#include "execution/CompilationUtils.h"

#include <QPlainTextEdit>
#include <QSpinBox>
#include <QCloseEvent>
#include <QFont>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QWidget(parent, Qt::Window) {
    setWindowTitle("Settings");
    setObjectName("SettingsWindow");
    setMinimumSize(500, 400);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setupUi();
}

void SettingsDialog::setupUi() {
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    tabWidget_ = new QTabWidget(this);
    tabWidget_->addTab(createGeneralTab(), "General");
    tabWidget_->addTab(createTemplateTab(), "Template");
    tabWidget_->addTab(createLanguagesTab(), "Languages");
    tabWidget_->addTab(createExperimentalTab(), "Experimental");
    tabWidget_->addTab(createAboutTab(), "About");
    
    mainLayout->addWidget(tabWidget_);

    // Button bar
    auto *buttonBar = new QWidget(this);
    auto *buttonLayout = new QHBoxLayout(buttonBar);
    buttonLayout->setContentsMargins(12, 12, 12, 12);
    buttonLayout->setSpacing(8);

    buttonLayout->addStretch();

    auto *cancelBtn = new QPushButton("Cancel", buttonBar);
    connect(cancelBtn, &QPushButton::clicked, this, [this]() {
        emit cancelled();
        close();
    });
    buttonLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton("Save", buttonBar);
    saveBtn->setObjectName("PrimaryAction");
    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        emit saved();
        close();
    });
    buttonLayout->addWidget(saveBtn);

    mainLayout->addWidget(buttonBar);
}

QWidget* SettingsDialog::createGeneralTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *pathGroup = new QGroupBox("File Explorer", widget);
    auto *pathLayout = new QFormLayout(pathGroup);

    rootDirEdit_ = new QLineEdit(pathGroup);
    rootDirEdit_->setPlaceholderText("Current working directory");
    rootDirEdit_->setReadOnly(true);
    rootDirEdit_->setCursor(Qt::PointingHandCursor);
    rootDirEdit_->installEventFilter(this);

    pathLayout->addRow("Root Directory:", rootDirEdit_);
    layout->addWidget(pathGroup);

    auto *autosaveGroup = new QGroupBox("Autosave", widget);
    auto *autosaveLayout = new QFormLayout(autosaveGroup);
    autosaveLayout->setContentsMargins(12, 12, 12, 12);
    autosaveLayout->setSpacing(8);

    autosaveIntervalSpin_ = new QSpinBox(autosaveGroup);
    autosaveIntervalSpin_->setRange(5, 300);
    autosaveIntervalSpin_->setSingleStep(5);
    autosaveIntervalSpin_->setSuffix(" s");
    autosaveIntervalSpin_->setValue(15);
    connect(autosaveIntervalSpin_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SettingsDialog::settingsChanged);

    autosaveLayout->addRow("Autosave every:", autosaveIntervalSpin_);
    layout->addWidget(autosaveGroup);
    layout->addStretch();

    return widget;
}

QWidget* SettingsDialog::createTemplateTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *transclusionGroup = new QGroupBox("Template Transclusion", widget);
    auto *transclusionLayout = new QVBoxLayout(transclusionGroup);

    auto *note = new QLabel(
        "Template transclusion auto-inserts your solution at the //#main marker.",
        transclusionGroup);
    note->setWordWrap(true);
    transclusionLayout->addWidget(note);

    transcludeTemplateCheckbox_ =
        new QCheckBox("Enable template transclusion", transclusionGroup);
    transcludeTemplateCheckbox_->setChecked(false);
    connect(transcludeTemplateCheckbox_, &QCheckBox::toggled,
            this, &SettingsDialog::settingsChanged);
    transclusionLayout->addWidget(transcludeTemplateCheckbox_);

    auto createTemplateGroup = [&](const QString &title) -> QPlainTextEdit * {
        auto *group = new QGroupBox(title, widget);
        auto *groupLayout = new QVBoxLayout(group);
        auto *editor = new QPlainTextEdit(group);
        editor->setPlaceholderText("//#main");
        editor->setMinimumHeight(90);
        editor->setFont(QFont("monospace", 10));
        connect(editor, &QPlainTextEdit::textChanged,
                this, &SettingsDialog::settingsChanged);
        groupLayout->addWidget(editor);
        layout->addWidget(group);
        return editor;
    };

    layout->addWidget(transclusionGroup);
    cppTemplateEdit_ = createTemplateGroup("C++");
    pythonTemplateEdit_ = createTemplateGroup("Python");
    javaTemplateEdit_ = createTemplateGroup("Java");

    layout->addStretch();

    return widget;
}

QWidget* SettingsDialog::createLanguagesTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *defaultGroup = new QGroupBox("Default Language", widget);
    auto *defaultLayout = new QFormLayout(defaultGroup);

    defaultLanguageCombo_ = new QComboBox(defaultGroup);
    defaultLanguageCombo_->addItem("C++");
    defaultLanguageCombo_->addItem("Python");
    defaultLanguageCombo_->addItem("Java");
    defaultLanguageCombo_->setCurrentIndex(0);
    connect(defaultLanguageCombo_, &QComboBox::currentTextChanged,
            this, &SettingsDialog::settingsChanged);
    defaultLayout->addRow("Default language:", defaultLanguageCombo_);

    auto *cppGroup = new QGroupBox("C++", widget);
    auto *cppLayout = new QFormLayout(cppGroup);

    cppCompilerPathEdit_ = new QLineEdit(cppGroup);
    cppCompilerPathEdit_->setPlaceholderText("g++");
    cppLayout->addRow("Compiler Path:", cppCompilerPathEdit_);

    cppCompilerFlagsEdit_ = new QLineEdit(cppGroup);
    cppCompilerFlagsEdit_->setPlaceholderText("-O2 -std=c++17");
    cppLayout->addRow("Compiler Flags:", cppCompilerFlagsEdit_);

    auto *pythonGroup = new QGroupBox("Python", widget);
    auto *pythonLayout = new QFormLayout(pythonGroup);

    pythonPathEdit_ = new QLineEdit(pythonGroup);
    pythonPathEdit_->setPlaceholderText("python3");
    pythonLayout->addRow("Interpreter Path:", pythonPathEdit_);

    pythonArgsEdit_ = new QLineEdit(pythonGroup);
    pythonArgsEdit_->setPlaceholderText("-O");
    pythonLayout->addRow("Run Args:", pythonArgsEdit_);

    auto *javaGroup = new QGroupBox("Java", widget);
    auto *javaLayout = new QFormLayout(javaGroup);

    javaCompilerPathEdit_ = new QLineEdit(javaGroup);
    javaCompilerPathEdit_->setPlaceholderText("javac");
    javaLayout->addRow("Compiler Path:", javaCompilerPathEdit_);

    javaRunPathEdit_ = new QLineEdit(javaGroup);
    javaRunPathEdit_->setPlaceholderText("java");
    javaLayout->addRow("Runtime Path:", javaRunPathEdit_);

    javaArgsEdit_ = new QLineEdit(javaGroup);
    javaArgsEdit_->setPlaceholderText("");
    javaLayout->addRow("Run Args:", javaArgsEdit_);

    layout->addWidget(defaultGroup);
    layout->addWidget(cppGroup);
    layout->addWidget(pythonGroup);
    layout->addWidget(javaGroup);
    layout->addStretch();

    return widget;
}

QWidget* SettingsDialog::createExperimentalTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *warningLabel = new QLabel(
        "<b>⚠ Experimental Features</b><br>"
        "These features are experimental and may cause instability.");
    warningLabel->setWordWrap(true);
    warningLabel->setTextFormat(Qt::RichText);
    layout->addWidget(warningLabel);

    layout->addSpacing(12);

    auto *perfGroup = new QGroupBox("Multithreading", widget);
    auto *perfLayout = new QVBoxLayout(perfGroup);

    multithreadingCheckbox_ = new QCheckBox("Enable parallel test execution", perfGroup);
    multithreadingCheckbox_->setToolTip(
        "Run multiple test cases in parallel instead of sequentially.\n"
        "This can significantly speed up testing for problems with many test cases.");
    perfLayout->addWidget(multithreadingCheckbox_);
    connect(multithreadingCheckbox_, &QCheckBox::toggled,
            this, &SettingsDialog::onMultithreadingToggled);

    layout->addWidget(perfGroup);
    layout->addStretch();

    return widget;
}

QWidget* SettingsDialog::createAboutTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(20, 20, 20, 20);

    auto *titleLabel = new QLabel(QString("<h2>CF Dojo %1</h2>").arg(CFDojo::versionString()));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto *descLabel = new QLabel(
        "A desktop application for practicing competitive programming problems.\n\n"
        "Features:\n"
        "• Import problems from Competitive Companion\n"
        "• Automatic test case validation\n"
        "• Stress testing (generator + brute)\n"
        "• Template transclusion with //#main marker\n"
        "• Save/load problems as .cpack files");
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    layout->addSpacing(12);

    auto *noteLabel = new QLabel(
        "Help and credits are available under Help \u2192 About.", widget);
    noteLabel->setWordWrap(true);
    noteLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(noteLabel);

    layout->addStretch();

    return widget;
}


void SettingsDialog::setTemplateForLanguage(const QString &language, const QString &tmpl) {
    if (auto *editor = editorForLanguage(language)) {
        QSignalBlocker blocker(editor);
        editor->setPlainText(tmpl);
    }
}

QString SettingsDialog::getTemplateForLanguage(const QString &language) const {
    if (const auto *editor = editorForLanguage(language)) {
        const QString text = editor->toPlainText();
        if (!text.isEmpty()) return text;
    }
    return QString{CompilationUtils::kDefaultTemplateCode};
}

QPlainTextEdit *SettingsDialog::editorForLanguage(const QString &language) const {
    if (language == QLatin1String("Python")) return pythonTemplateEdit_;
    if (language == QLatin1String("Java"))   return javaTemplateEdit_;
    return cppTemplateEdit_;
}

void SettingsDialog::setTranscludeTemplateEnabled(bool enabled) {
    if (!transcludeTemplateCheckbox_) {
        return;
    }
    QSignalBlocker blocker(transcludeTemplateCheckbox_);
    transcludeTemplateCheckbox_->setChecked(enabled);
}

bool SettingsDialog::isTranscludeTemplateEnabled() const {
    return transcludeTemplateCheckbox_ ? transcludeTemplateCheckbox_->isChecked() : false;
}

void SettingsDialog::setCompilerPath(const QString &path) {
    if (cppCompilerPathEdit_) {
        cppCompilerPathEdit_->setText(path);
    }
}

QString SettingsDialog::compilerPath() const {
    return cppCompilerPathEdit_ ? cppCompilerPathEdit_->text() : "g++";
}

void SettingsDialog::setCompilerFlags(const QString &flags) {
    if (cppCompilerFlagsEdit_) {
        cppCompilerFlagsEdit_->setText(flags);
    }
}

QString SettingsDialog::compilerFlags() const {
    return cppCompilerFlagsEdit_ ? cppCompilerFlagsEdit_->text() : "";
}

void SettingsDialog::setDefaultLanguage(const QString &language) {
    if (!defaultLanguageCombo_) {
        return;
    }
    const int idx = defaultLanguageCombo_->findText(language);
    if (idx >= 0) {
        defaultLanguageCombo_->setCurrentIndex(idx);
    }
}

QString SettingsDialog::defaultLanguage() const {
    return defaultLanguageCombo_ ? defaultLanguageCombo_->currentText() : "C++";
}

void SettingsDialog::setPythonPath(const QString &path) {
    if (pythonPathEdit_) {
        pythonPathEdit_->setText(path);
    }
}

QString SettingsDialog::pythonPath() const {
    return pythonPathEdit_ ? pythonPathEdit_->text() : "python3";
}

void SettingsDialog::setPythonArgs(const QString &args) {
    if (pythonArgsEdit_) {
        pythonArgsEdit_->setText(args);
    }
}

QString SettingsDialog::pythonArgs() const {
    return pythonArgsEdit_ ? pythonArgsEdit_->text() : "";
}

void SettingsDialog::setJavaCompilerPath(const QString &path) {
    if (javaCompilerPathEdit_) {
        javaCompilerPathEdit_->setText(path);
    }
}

QString SettingsDialog::javaCompilerPath() const {
    return javaCompilerPathEdit_ ? javaCompilerPathEdit_->text() : "javac";
}

void SettingsDialog::setJavaRunPath(const QString &path) {
    if (javaRunPathEdit_) {
        javaRunPathEdit_->setText(path);
    }
}

QString SettingsDialog::javaRunPath() const {
    return javaRunPathEdit_ ? javaRunPathEdit_->text() : "java";
}

void SettingsDialog::setJavaArgs(const QString &args) {
    if (javaArgsEdit_) {
        javaArgsEdit_->setText(args);
    }
}

QString SettingsDialog::javaArgs() const {
    return javaArgsEdit_ ? javaArgsEdit_->text() : "";
}

void SettingsDialog::setRootDir(const QString &path) {
    if (rootDirEdit_) {
        rootDirEdit_->setText(path);
    }
}

QString SettingsDialog::rootDir() const {
    return rootDirEdit_ ? rootDirEdit_->text() : "";
}

void SettingsDialog::setAutosaveIntervalSeconds(int seconds) {
    if (!autosaveIntervalSpin_) {
        return;
    }
    QSignalBlocker blocker(autosaveIntervalSpin_);
    autosaveIntervalSpin_->setValue(seconds);
}

int SettingsDialog::autosaveIntervalSeconds() const {
    return autosaveIntervalSpin_ ? autosaveIntervalSpin_->value() : 15;
}

void SettingsDialog::setMultithreadingEnabled(bool enabled) {
    if (multithreadingCheckbox_) {
        multithreadingCheckbox_->setChecked(enabled);
    }
}

bool SettingsDialog::isMultithreadingEnabled() const {
    return multithreadingCheckbox_ ? multithreadingCheckbox_->isChecked() : false;
}

void SettingsDialog::onMultithreadingToggled(bool checked) {
    if (!checked || !multithreadingCheckbox_) {
        return;
    }

    QSettings settings("CF Dojo", "CF Dojo");
    const bool suppressed = settings.value("multithreadingWarningSuppressed", false).toBool();
    if (suppressed) {
        return;
    }

    QMessageBox box(this);
    box.setWindowTitle("Multithreading Warning");
    box.setIcon(QMessageBox::Warning);
    box.setText("Multithreading is experimental.");
    box.setInformativeText(
        "Parallel execution can increase CPU usage and may cause instability.\n"
        "Use this option only if you understand the trade-offs.");
    QCheckBox *dontShow = new QCheckBox("Don't show this again", &box);
    box.setCheckBox(dontShow);
    box.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    box.setDefaultButton(QMessageBox::Ok);

    const int result = box.exec();
    if (dontShow->isChecked()) {
        settings.setValue("multithreadingWarningSuppressed", true);
    }
    if (result == QMessageBox::Cancel) {
        QSignalBlocker blocker(multithreadingCheckbox_);
        multithreadingCheckbox_->setChecked(false);
        return;
    }
}

bool SettingsDialog::eventFilter(QObject *obj, QEvent *event) {
    if (obj == rootDirEdit_ && event->type() == QEvent::MouseButtonPress) {
        QString dir = QFileDialog::getExistingDirectory(
            this, "Select Root Directory",
            rootDirEdit_->text().isEmpty() ? QDir::currentPath() : rootDirEdit_->text());
        if (!dir.isEmpty()) {
            rootDirEdit_->setText(dir);
        }
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void SettingsDialog::closeEvent(QCloseEvent *event) {
    emit closed();
    QWidget::closeEvent(event);
}
