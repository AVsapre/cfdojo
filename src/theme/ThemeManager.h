#pragma once

#include <QColor>
#include <QString>

class QApplication;

// Theme color palette for VS Code-like dark theme
struct ThemeColors {
    QColor background{"#1e1e1e"};
    QColor edge{"#3e3e42"};
    QColor text{"#d4d4d4"};
    QColor selection{"#264f78"};
    QColor caretLine{"#2a2a2a"};
    
    // Syntax highlighting
    QColor comment{"#6A9955"};
    QColor number{"#B5CEA8"};
    QColor keyword{"#569CD6"};
    QColor keyword2{"#4EC9B0"};
    QColor string{"#CE9178"};
    QColor preprocessor{"#C586C0"};
    QColor error{"#F44747"};
    QColor regex{"#D16969"};
    QColor escape{"#D7BA7D"};
    QColor docKeyword{"#C586C0"};
};

class ThemeManager {
public:
    ThemeManager() = default;

    const ThemeColors &colors() const { return colors_; }
    
    QColor backgroundColor() const { return colors_.background; }
    QColor edgeColor() const { return colors_.edge; }
    QColor textColor() const { return colors_.text; }
    QColor selectionBackground() const { return colors_.selection; }
    QColor caretLineBackground() const { return colors_.caretLine; }
    bool isDarkTheme() const { return colors_.background.lightness() < 128; }

    void apply(QApplication *app, double scale = 1.0) const;

private:
    QString generateZoomOverrides(double scale) const;
    
    ThemeColors colors_;
};
