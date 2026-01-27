#include "app/SettingsDialog.h"
#include "editor/DojoCppLexer.h"
#include "theme/ThemeManager.h"

#include <QCheckBox>
#include <QDir>
#include <QDialog>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QComboBox>
#include <QCloseEvent>
#include <Qsci/qsciscintilla.h>
#include <Qsci/qscilexercpp.h>
#include <QFont>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
void applyTemplateEditorTheme(QsciScintilla *editor) {
    if (!editor) {
        return;
    }

    ThemeManager theme;
    const ThemeColors &colors = theme.colors();
    const QColor bg = colors.background;
    const QColor fg = colors.text;

    QFont font("Consolas", 11);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);

    editor->setFont(font);
    editor->setMarginsFont(font);
    editor->setPaper(bg);
    editor->setColor(fg);
    editor->setCaretForegroundColor(fg);
    editor->setCaretLineVisible(true);
    editor->setCaretLineBackgroundColor(theme.caretLineBackground());
    editor->setSelectionBackgroundColor(theme.selectionBackground());
    editor->SendScintilla(QsciScintilla::SCI_SETSELFORE, 0UL, 0L);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor->setScrollWidthTracking(true);
    editor->setScrollWidth(1);
    editor->setExtraAscent(2);
    editor->setExtraDescent(2);
    editor->setEdgeMode(QsciScintilla::EdgeNone);
    editor->setMarginsBackgroundColor(bg);
    editor->setMarginsForegroundColor(fg);

    auto *lexer = new DojoCppLexer(editor);
    editor->setLexer(lexer);
    lexer->setDefaultFont(font);
    lexer->setDefaultColor(fg);
    lexer->setDefaultPaper(bg);
    lexer->setFont(font);
    lexer->setColor(fg, QsciLexerCPP::Default);
    lexer->setPaper(bg, QsciLexerCPP::Default);
    lexer->setFoldAtElse(true);
    lexer->setFoldComments(true);
    lexer->setFoldCompact(true);
    lexer->setFoldPreprocessor(true);

    if (theme.isDarkTheme()) {
        auto setStyle = [lexer, &bg](int style, int inactive, const QColor &color) {
            lexer->setColor(color, style);
            lexer->setColor(color, inactive);
            lexer->setPaper(bg, style);
            lexer->setPaper(bg, inactive);
        };

        setStyle(QsciLexerCPP::Default, QsciLexerCPP::InactiveDefault, colors.text);
        setStyle(QsciLexerCPP::Identifier, QsciLexerCPP::InactiveIdentifier, colors.text);
        setStyle(QsciLexerCPP::Operator, QsciLexerCPP::InactiveOperator, colors.text);
        setStyle(QsciLexerCPP::Comment, QsciLexerCPP::InactiveComment, colors.comment);
        setStyle(QsciLexerCPP::CommentLine, QsciLexerCPP::InactiveCommentLine, colors.comment);
        setStyle(QsciLexerCPP::CommentDoc, QsciLexerCPP::InactiveCommentDoc, colors.comment);
        setStyle(QsciLexerCPP::CommentLineDoc, QsciLexerCPP::InactiveCommentLineDoc, colors.comment);
        setStyle(QsciLexerCPP::PreProcessorComment,
                 QsciLexerCPP::InactivePreProcessorComment,
                 colors.comment);
        setStyle(QsciLexerCPP::PreProcessorCommentLineDoc,
                 QsciLexerCPP::InactivePreProcessorCommentLineDoc,
                 colors.comment);
        setStyle(QsciLexerCPP::CommentDocKeyword,
                 QsciLexerCPP::InactiveCommentDocKeyword,
                 colors.docKeyword);
        setStyle(QsciLexerCPP::CommentDocKeywordError,
                 QsciLexerCPP::InactiveCommentDocKeywordError,
                 colors.error);
        setStyle(QsciLexerCPP::Number, QsciLexerCPP::InactiveNumber, colors.number);
        setStyle(QsciLexerCPP::UUID, QsciLexerCPP::InactiveUUID, colors.number);
        setStyle(QsciLexerCPP::Keyword, QsciLexerCPP::InactiveKeyword, colors.keyword);
        setStyle(QsciLexerCPP::KeywordSet2, QsciLexerCPP::InactiveKeywordSet2, colors.keyword2);
        setStyle(QsciLexerCPP::GlobalClass,
                 QsciLexerCPP::InactiveGlobalClass,
                 colors.keyword2);
        setStyle(QsciLexerCPP::DoubleQuotedString,
                 QsciLexerCPP::InactiveDoubleQuotedString,
                 colors.string);
        setStyle(QsciLexerCPP::SingleQuotedString,
                 QsciLexerCPP::InactiveSingleQuotedString,
                 colors.string);
        setStyle(QsciLexerCPP::RawString, QsciLexerCPP::InactiveRawString, colors.string);
        setStyle(QsciLexerCPP::VerbatimString,
                 QsciLexerCPP::InactiveVerbatimString,
                 colors.string);
        setStyle(QsciLexerCPP::TripleQuotedVerbatimString,
                 QsciLexerCPP::InactiveTripleQuotedVerbatimString,
                 colors.string);
        setStyle(QsciLexerCPP::HashQuotedString,
                 QsciLexerCPP::InactiveHashQuotedString,
                 colors.string);
        setStyle(QsciLexerCPP::UserLiteral,
                 QsciLexerCPP::InactiveUserLiteral,
                 colors.string);
        setStyle(QsciLexerCPP::PreProcessor,
                 QsciLexerCPP::InactivePreProcessor,
                 colors.preprocessor);
        setStyle(QsciLexerCPP::UnclosedString,
                 QsciLexerCPP::InactiveUnclosedString,
                 colors.error);
        setStyle(QsciLexerCPP::Regex, QsciLexerCPP::InactiveRegex, colors.regex);
        setStyle(QsciLexerCPP::EscapeSequence,
                 QsciLexerCPP::InactiveEscapeSequence,
                 colors.escape);
        setStyle(QsciLexerCPP::TaskMarker,
                 QsciLexerCPP::InactiveTaskMarker,
                 colors.docKeyword);
    }
}
} // namespace

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
    layout->addStretch();

    return widget;
}

QWidget* SettingsDialog::createTemplateTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *templateGroup = new QGroupBox("Template", widget);
    auto *groupLayout = new QVBoxLayout(templateGroup);
    groupLayout->setContentsMargins(12, 12, 12, 12);
    groupLayout->setSpacing(8);

    auto *formLayout = new QFormLayout();
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setSpacing(8);

    templateSummaryEdit_ = new QLineEdit(templateGroup);
    templateSummaryEdit_->setReadOnly(true);
    templateSummaryEdit_->setPlaceholderText("Click to edit template");
    templateSummaryEdit_->setCursor(Qt::PointingHandCursor);
    templateSummaryEdit_->installEventFilter(this);

    formLayout->addRow("Default template:", templateSummaryEdit_);

    groupLayout->addLayout(formLayout);

    transcludeTemplateCheckbox_ = new QCheckBox("Transclude template", templateGroup);
    transcludeTemplateCheckbox_->setChecked(false);
    connect(transcludeTemplateCheckbox_, &QCheckBox::toggled,
            this, &SettingsDialog::settingsChanged);
    groupLayout->addWidget(transcludeTemplateCheckbox_);

    layout->addWidget(templateGroup);
    layout->addStretch();
    updateTemplateSummary();

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

    auto *titleLabel = new QLabel("<h2>CF Dojo</h2>");
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

    auto *helpGroup = new QGroupBox("Help", widget);
    auto *helpLayout = new QVBoxLayout(helpGroup);
    auto *helpLabel = new QLabel("Tooltips across the app provide quick usage hints.", helpGroup);
    helpLabel->setWordWrap(true);
    helpLayout->addWidget(helpLabel);
    layout->addWidget(helpGroup);

    auto *creditsGroup = new QGroupBox("Credits", widget);
    auto *creditsLayout = new QVBoxLayout(creditsGroup);
    auto *creditsLabel = new QLabel(creditsGroup);
    creditsLabel->setTextFormat(Qt::RichText);
    creditsLabel->setOpenExternalLinks(true);
    creditsLabel->setWordWrap(true);
    creditsLabel->setText(
        "Icon sources (The Noun Project):"
        "<br><span style=\"opacity:0.8;\">License: CC BY 3.0</span>"
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
    creditsLayout->addWidget(creditsLabel);
    layout->addWidget(creditsGroup);

    layout->addStretch();

    return widget;
}


void SettingsDialog::setTemplate(const QString &tmpl) {
    templateText_ = tmpl;
    updateTemplateSummary();
}

QString SettingsDialog::getTemplate() const {
    return templateText_.isEmpty() ? QString("//#main") : templateText_;
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
    const bool warned = settings.value("multithreadingWarningShown", false).toBool();
    const bool suppressed = settings.value("multithreadingWarningSuppressed", false).toBool();
    if (warned || suppressed) {
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

    settings.setValue("multithreadingWarningShown", true);
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
    if (obj == templateSummaryEdit_ && event->type() == QEvent::MouseButtonPress) {
        openTemplateEditorDialog();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

void SettingsDialog::closeEvent(QCloseEvent *event) {
    emit closed();
    QWidget::closeEvent(event);
}

void SettingsDialog::openTemplateEditorDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("Template");
    dialog.setMinimumSize(640, 480);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    ThemeManager theme;
    const ThemeColors &colors = theme.colors();
    const QColor panelBg = colors.background;
    const QColor panelEdge = colors.edge;

    auto *contentPanel = new QWidget(&dialog);
    contentPanel->setObjectName("TemplateEditorPanel");
    contentPanel->setAttribute(Qt::WA_StyledBackground, true);
    contentPanel->setStyleSheet(
        QString("QWidget#TemplateEditorPanel { background: %1; }").arg(panelBg.name()));

    auto *contentLayout = new QVBoxLayout(contentPanel);
    contentLayout->setContentsMargins(12, 12, 12, 12);
    contentLayout->setSpacing(12);

    auto *editor = new QsciScintilla(contentPanel);
    editor->setUtf8(true);
    editor->setText(templateText_.isEmpty() ? QString("//#main") : templateText_);
    editor->setTabWidth(4);
    editor->setIndentationWidth(4);
    editor->setIndentationsUseTabs(true);
    editor->setTabIndents(true);
    editor->setBackspaceUnindents(true);
    editor->setMarginWidth(0, 0);
    editor->setMarginWidth(1, 0);
    editor->setMarginWidth(2, 0);
    editor->setFolding(QsciScintilla::NoFoldStyle);
    editor->setWrapMode(QsciScintilla::WrapNone);
    applyTemplateEditorTheme(editor);
    editor->setStyleSheet(QString("QsciScintilla { background: %1; border: 1px solid %2; }")
                               .arg(panelBg.name(), panelEdge.name()));
    contentLayout->addWidget(editor, 1);

    auto *buttonRow = new QWidget(contentPanel);
    auto *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(8);

    auto *tipLabel = new QLabel("Your solution is inserted at the //#main marker in the template.", buttonRow);
    tipLabel->setWordWrap(false);
    tipLabel->setStyleSheet("color: #9aa0a6; font-size: 11px;");
    buttonLayout->addWidget(tipLabel);
    buttonLayout->addStretch();

    auto *cancelBtn = new QPushButton("Cancel", buttonRow);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    buttonLayout->addWidget(cancelBtn);

    auto *saveBtn = new QPushButton("Save", buttonRow);
    saveBtn->setObjectName("PrimaryAction");
    connect(saveBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addWidget(saveBtn);

    contentLayout->addWidget(buttonRow);
    layout->addWidget(contentPanel, 1);

    if (dialog.exec() == QDialog::Accepted) {
        templateText_ = editor->text();
        updateTemplateSummary();
        emit settingsChanged();
    }
}

void SettingsDialog::updateTemplateSummary() {
    if (!templateSummaryEdit_) {
        return;
    }
    templateSummaryEdit_->setText(QString());
}
