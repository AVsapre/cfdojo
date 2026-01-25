#pragma once

#include <QFont>
#include <QObject>

class QWidget;
class QsciScintilla;
class QsciLexerCPP;
class QsciAPIs;
class ThemeManager;
class EditorPlaceholder;

class EditorConfigurator : public QObject {
    Q_OBJECT

public:
    struct EditorWidgets {
        QWidget *container = nullptr;
        QsciScintilla *editor = nullptr;
    };

    explicit EditorConfigurator(QObject *parent = nullptr);
    
    EditorWidgets build(QWidget *parent, const ThemeManager &theme);
    void applyZoom(double scale);

private:
    void setupEditor(const ThemeManager &theme);
    void setupLexer(const ThemeManager &theme);
    void setupMargins(const ThemeManager &theme);
    void setupFolding(const ThemeManager &theme);
    void setupAutoComplete();
    void applySyntaxColors(const ThemeManager &theme);
    void updateMarginWidth();
    void resetScrollWidth();

    QsciScintilla *editor_ = nullptr;
    EditorPlaceholder *placeholder_ = nullptr;
    QsciLexerCPP *lexer_ = nullptr;
    QsciAPIs *apis_ = nullptr;
    QFont baseFont_;
};
