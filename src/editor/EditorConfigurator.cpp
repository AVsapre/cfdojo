#include "editor/EditorConfigurator.h"
#include "editor/DojoCppLexer.h"
#include "editor/EditorPlaceholder.h"
#include "theme/ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QGridLayout>
#include <QTextStream>
#include <QVBoxLayout>
#include <Qsci/qsciapis.h>
#include <Qsci/qsciscintilla.h>
#include <algorithm>

EditorConfigurator::EditorConfigurator(QObject *parent)
    : QObject(parent) {}

EditorConfigurator::EditorWidgets EditorConfigurator::build(QWidget *parent,
                                                            const ThemeManager &theme) {
    editor_ = new QsciScintilla(parent);
    editor_->setUtf8(true);

    setupEditor(theme);
    setupLexer(theme);
    setupAutoComplete();
    setupMargins(theme);
    setupFolding(theme);

    if (theme.isDarkTheme()) {
        applySyntaxColors(theme);
    }

    // Create container with overlay for placeholder
    QWidget *container = new QWidget(parent);
    QVBoxLayout *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 12, 0, 0);

    QWidget *overlay = new QWidget(container);
    QGridLayout *overlayLayout = new QGridLayout(overlay);
    overlayLayout->setContentsMargins(0, 0, 0, 0);
    overlayLayout->setSpacing(0);
    overlayLayout->addWidget(editor_, 0, 0);

    // Create placeholder using the new widget
    placeholder_ = new EditorPlaceholder(editor_, "Write your solution here...");
    placeholder_->setFont(baseFont_);

    layout->addWidget(overlay);

    return {container, editor_};
}

void EditorConfigurator::setupEditor(const ThemeManager &theme) {
    const QColor bg = theme.backgroundColor();
    const QColor fg = theme.textColor();

    // Setup font
    QFont font("Consolas", 11);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    baseFont_ = font;
    editor_->setFont(font);
    editor_->setMarginsFont(font);

    // Colors
    editor_->setPaper(bg);
    editor_->setColor(fg);
    editor_->setCaretForegroundColor(fg);
    editor_->setCaretLineVisible(true);
    editor_->setCaretLineBackgroundColor(theme.caretLineBackground());
    editor_->setSelectionBackgroundColor(theme.selectionBackground());
    
    // Disable selection foreground override
    editor_->SendScintilla(QsciScintilla::SCI_SETSELFORE, 0UL, 0L);

    // Scrolling
    editor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor_->setScrollWidthTracking(true);
    editor_->setScrollWidth(1);

    // Line spacing
    editor_->setExtraAscent(2);
    editor_->setExtraDescent(2);

    // Reset scroll width on text change
    connect(editor_, &QsciScintilla::textChanged, this, &EditorConfigurator::resetScrollWidth);
}

void EditorConfigurator::setupLexer(const ThemeManager &theme) {
    lexer_ = new DojoCppLexer(editor_);
    editor_->setLexer(lexer_);

    const QColor bg = theme.backgroundColor();
    const QColor fg = theme.textColor();

    lexer_->setDefaultFont(baseFont_);
    lexer_->setDefaultColor(fg);
    lexer_->setDefaultPaper(bg);
    lexer_->setFont(baseFont_);
    lexer_->setColor(fg, QsciLexerCPP::Default);
    lexer_->setPaper(bg, QsciLexerCPP::Default);
    lexer_->setFoldAtElse(true);
    lexer_->setFoldComments(true);
    lexer_->setFoldCompact(true);
    lexer_->setFoldPreprocessor(true);
}

void EditorConfigurator::setupAutoComplete() {
    apis_ = new QsciAPIs(lexer_);
    
    QFile apiFile(":/keywords.txt");
    if (apiFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&apiFile);
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                apis_->add(line);
            }
        }
        apis_->prepare();
    }

    editor_->setAutoCompletionSource(QsciScintilla::AcsAPIs);
    editor_->setAutoCompletionThreshold(1);
    editor_->setAutoCompletionCaseSensitivity(false);
}

void EditorConfigurator::setupMargins(const ThemeManager &theme) {
    const QColor bg = theme.backgroundColor();
    const QColor fg = theme.textColor();

    // Line number margin
    editor_->setMarginType(0, QsciScintilla::NumberMargin);
    editor_->setMarginLineNumbers(0, true);
    editor_->setMarginsForegroundColor(fg);
    editor_->setMarginsBackgroundColor(bg);

    updateMarginWidth();
    connect(editor_, &QsciScintilla::linesChanged, this, &EditorConfigurator::updateMarginWidth);
}

void EditorConfigurator::setupFolding(const ThemeManager &theme) {
    const QColor bg = theme.backgroundColor();
    const QColor fg = theme.textColor();

    constexpr int foldMargin = 1;
    editor_->setMarginType(foldMargin, QsciScintilla::SymbolMargin);
    editor_->setMarginWidth(foldMargin, 12);
    editor_->setMarginSensitivity(foldMargin, true);
    editor_->setMarginBackgroundColor(foldMargin, bg);
    editor_->setFolding(QsciScintilla::BoxedTreeFoldStyle, foldMargin);
    editor_->setFoldMarginColors(bg, bg);

    constexpr int foldMarkers[] = {
        QsciScintilla::SC_MARKNUM_FOLDEREND,
        QsciScintilla::SC_MARKNUM_FOLDEROPENMID,
        QsciScintilla::SC_MARKNUM_FOLDERMIDTAIL,
        QsciScintilla::SC_MARKNUM_FOLDERTAIL,
        QsciScintilla::SC_MARKNUM_FOLDERSUB,
        QsciScintilla::SC_MARKNUM_FOLDER,
        QsciScintilla::SC_MARKNUM_FOLDEROPEN
    };
    for (int marker : foldMarkers) {
        editor_->setMarkerBackgroundColor(bg, marker);
        editor_->setMarkerForegroundColor(fg, marker);
    }
}

void EditorConfigurator::applySyntaxColors(const ThemeManager &theme) {
    const ThemeColors &colors = theme.colors();
    const QColor bg = colors.background;

    auto setStyle = [this, &bg](int style, int inactive, const QColor &color) {
        lexer_->setColor(color, style);
        lexer_->setColor(color, inactive);
        lexer_->setPaper(bg, style);
        lexer_->setPaper(bg, inactive);
    };

    // Default/identifiers
    setStyle(QsciLexerCPP::Default, QsciLexerCPP::InactiveDefault, colors.text);
    setStyle(QsciLexerCPP::Identifier, QsciLexerCPP::InactiveIdentifier, colors.text);
    setStyle(QsciLexerCPP::Operator, QsciLexerCPP::InactiveOperator, colors.text);

    // Comments
    setStyle(QsciLexerCPP::Comment, QsciLexerCPP::InactiveComment, colors.comment);
    setStyle(QsciLexerCPP::CommentLine, QsciLexerCPP::InactiveCommentLine, colors.comment);
    setStyle(QsciLexerCPP::CommentDoc, QsciLexerCPP::InactiveCommentDoc, colors.comment);
    setStyle(QsciLexerCPP::CommentLineDoc, QsciLexerCPP::InactiveCommentLineDoc, colors.comment);
    setStyle(QsciLexerCPP::PreProcessorComment, QsciLexerCPP::InactivePreProcessorComment, colors.comment);
    setStyle(QsciLexerCPP::PreProcessorCommentLineDoc, QsciLexerCPP::InactivePreProcessorCommentLineDoc, colors.comment);

    // Doc keywords
    setStyle(QsciLexerCPP::CommentDocKeyword, QsciLexerCPP::InactiveCommentDocKeyword, colors.docKeyword);
    setStyle(QsciLexerCPP::CommentDocKeywordError, QsciLexerCPP::InactiveCommentDocKeywordError, colors.error);

    // Numbers
    setStyle(QsciLexerCPP::Number, QsciLexerCPP::InactiveNumber, colors.number);
    setStyle(QsciLexerCPP::UUID, QsciLexerCPP::InactiveUUID, colors.number);

    // Keywords
    setStyle(QsciLexerCPP::Keyword, QsciLexerCPP::InactiveKeyword, colors.keyword);
    setStyle(QsciLexerCPP::KeywordSet2, QsciLexerCPP::InactiveKeywordSet2, colors.keyword2);
    setStyle(QsciLexerCPP::GlobalClass, QsciLexerCPP::InactiveGlobalClass, colors.keyword2);

    // Strings
    setStyle(QsciLexerCPP::DoubleQuotedString, QsciLexerCPP::InactiveDoubleQuotedString, colors.string);
    setStyle(QsciLexerCPP::SingleQuotedString, QsciLexerCPP::InactiveSingleQuotedString, colors.string);
    setStyle(QsciLexerCPP::RawString, QsciLexerCPP::InactiveRawString, colors.string);
    setStyle(QsciLexerCPP::VerbatimString, QsciLexerCPP::InactiveVerbatimString, colors.string);
    setStyle(QsciLexerCPP::TripleQuotedVerbatimString, QsciLexerCPP::InactiveTripleQuotedVerbatimString, colors.string);
    setStyle(QsciLexerCPP::HashQuotedString, QsciLexerCPP::InactiveHashQuotedString, colors.string);
    setStyle(QsciLexerCPP::UserLiteral, QsciLexerCPP::InactiveUserLiteral, colors.string);

    // Preprocessor
    setStyle(QsciLexerCPP::PreProcessor, QsciLexerCPP::InactivePreProcessor, colors.preprocessor);

    // Errors and special
    setStyle(QsciLexerCPP::UnclosedString, QsciLexerCPP::InactiveUnclosedString, colors.error);
    setStyle(QsciLexerCPP::Regex, QsciLexerCPP::InactiveRegex, colors.regex);
    setStyle(QsciLexerCPP::EscapeSequence, QsciLexerCPP::InactiveEscapeSequence, colors.escape);
    setStyle(QsciLexerCPP::TaskMarker, QsciLexerCPP::InactiveTaskMarker, colors.docKeyword);
}

void EditorConfigurator::updateMarginWidth() {
    if (!editor_) return;
    
    int digits = QString::number(editor_->lines()).length();
    digits = std::max(digits, 2);
    QString mask(digits, '9');
    mask.append(' ');
    editor_->setMarginWidth(0, mask);
}

void EditorConfigurator::resetScrollWidth() {
    if (!editor_) return;
    
    editor_->setScrollWidthTracking(false);
    editor_->setScrollWidth(1);
    editor_->setScrollWidthTracking(true);
}

void EditorConfigurator::applyZoom(double scale) {
    if (!editor_ || !lexer_) return;

    QFont scaledFont = baseFont_;
    scaledFont.setPointSizeF(baseFont_.pointSizeF() * scale);
    
    editor_->setFont(scaledFont);
    editor_->setMarginsFont(scaledFont);
    lexer_->setDefaultFont(scaledFont);
    lexer_->setFont(scaledFont);

    if (placeholder_) {
        placeholder_->setFont(scaledFont);
        placeholder_->adjustSize();
        placeholder_->updatePosition();
    }
    
    updateMarginWidth();
}
