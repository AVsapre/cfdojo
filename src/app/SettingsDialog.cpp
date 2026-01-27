#include "app/SettingsDialog.h"
#include "editor/DojoCppLexer.h"
#include "theme/ThemeManager.h"

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
    tabWidget_->addTab(createCompilerTab(), "Compiler");
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

    auto *label = new QLabel(
        "Template code with <code>//#main</code> transclusion marker.<br>"
        "When compiling, <code>//#main</code> is replaced with your solution code.");
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);
    layout->addWidget(label);

    templateEditor_ = new QsciScintilla(widget);
    templateEditor_->setUtf8(true);
    templateEditor_->setText("//#main");
    templateEditor_->setTabWidth(4);
    templateEditor_->setIndentationWidth(4);
    templateEditor_->setIndentationsUseTabs(true);
    templateEditor_->setTabIndents(true);
    templateEditor_->setBackspaceUnindents(true);
    templateEditor_->setMarginWidth(0, 0); // Hide line numbers for template
    templateEditor_->setMarginWidth(1, 0);
    templateEditor_->setMarginWidth(2, 0);
    templateEditor_->setFolding(QsciScintilla::NoFoldStyle);
    templateEditor_->setWrapMode(QsciScintilla::WrapNone);
    applyTemplateEditorTheme(templateEditor_);
    connect(templateEditor_, &QsciScintilla::textChanged,
            this, &SettingsDialog::settingsChanged);
    layout->addWidget(templateEditor_);

    return widget;
}

QWidget* SettingsDialog::createCompilerTab() {
    auto *widget = new QWidget();
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *compilerGroup = new QGroupBox("C++ Compiler", widget);
    auto *compilerLayout = new QFormLayout(compilerGroup);

    compilerPathEdit_ = new QLineEdit(compilerGroup);
    compilerPathEdit_->setPlaceholderText("g++");
    compilerLayout->addRow("Compiler Path:", compilerPathEdit_);

    compilerFlagsEdit_ = new QLineEdit(compilerGroup);
    compilerFlagsEdit_->setPlaceholderText("-O2 -std=c++17");
    compilerLayout->addRow("Compiler Flags:", compilerFlagsEdit_);

    layout->addWidget(compilerGroup);
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

    layout->addStretch();

    return widget;
}


void SettingsDialog::setTemplate(const QString &tmpl) {
    if (templateEditor_) {
        QSignalBlocker blocker(templateEditor_);
        templateEditor_->setText(tmpl);
    }
}


QString SettingsDialog::getTemplate() const {
    return templateEditor_ ? templateEditor_->text() : "//#main";
}

void SettingsDialog::setCompilerPath(const QString &path) {
    if (compilerPathEdit_) {
        compilerPathEdit_->setText(path);
    }
}

QString SettingsDialog::compilerPath() const {
    return compilerPathEdit_ ? compilerPathEdit_->text() : "g++";
}

void SettingsDialog::setCompilerFlags(const QString &flags) {
    if (compilerFlagsEdit_) {
        compilerFlagsEdit_->setText(flags);
    }
}

QString SettingsDialog::compilerFlags() const {
    return compilerFlagsEdit_ ? compilerFlagsEdit_->text() : "";
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
    return QWidget::eventFilter(obj, event);
}
