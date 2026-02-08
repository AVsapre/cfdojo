#include "theme/ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QString>
#include <QStyleFactory>
#include <algorithm>
#include <cmath>

namespace {
int scalePx(int value, double scale) {
    return std::max(1, static_cast<int>(std::round(value * scale)));
}

QColor lightenColor(const QColor &color, int percent) {
    return color.lighter(100 + percent);
}

QColor darkenColor(const QColor &color, int percent) {
    return color.darker(100 + percent);
}

struct ButtonPalette {
    QColor base;
    QColor hover;
    QColor pressed;
};

ButtonPalette makeButtonPalette(const QColor &base, int hoverDelta = 10, int pressedDelta = 12) {
    return {base, lightenColor(base, hoverDelta), darkenColor(base, pressedDelta)};
}

void applyButtonPalette(QString &style,
                        const QString &token,
                        const QColor &base,
                        int hoverDelta = 10,
                        int pressedDelta = 12) {
    const ButtonPalette palette = makeButtonPalette(base, hoverDelta, pressedDelta);
    style.replace(QString("@%1@").arg(token), palette.base.name());
    style.replace(QString("@%1Hover@").arg(token), palette.hover.name());
    style.replace(QString("@%1Pressed@").arg(token), palette.pressed.name());
}

void applyHoverPressed(QString &style,
                       const QString &token,
                       const QColor &base,
                       int hoverDelta = 10,
                       int pressedDelta = 12) {
    const ButtonPalette palette = makeButtonPalette(base, hoverDelta, pressedDelta);
    style.replace(QString("@%1Hover@").arg(token), palette.hover.name());
    style.replace(QString("@%1Pressed@").arg(token), palette.pressed.name());
}
} // namespace

void ThemeManager::apply(QApplication *app, double scale) const {
    if (!app) {
        return;
    }

    app->setStyle(QStyleFactory::create("Fusion"));

    QFile styleFile(":/style.qss");
    if (!styleFile.open(QFile::ReadOnly)) {
        return;
    }

    QString style = QString::fromUtf8(styleFile.readAll());
    
    // Replace color placeholders
    style.replace("@background@", colors_.background.name());
    style.replace("@edgecolor@", colors_.edge.name());
    style.replace("@textcolor@", colors_.text.name());
    style.replace("@appfont@", app->font().family());

    const QColor primaryBase("#0e639c");
    // Isoluminant green (H: 123°, S: 75%, L: 44%)
    const QColor successBase("#25a244");
    // Isoluminant red (H: 4°, S: 75%, L: 44%)
    const QColor dangerBase("#c42b1c");
    // Isoluminant blue (H: 210°, S: 75%, L: 44%)
    const QColor infoBase("#2176ae");

    applyHoverPressed(style, "bg", colors_.background, 10, 12);

    applyButtonPalette(style, "primary", primaryBase);
    applyButtonPalette(style, "success", successBase);
    applyButtonPalette(style, "danger", dangerBase);
    applyButtonPalette(style, "info", infoBase);
    applyButtonPalette(style, "greenbutton", successBase);
    applyButtonPalette(style, "redbutton", dangerBase);
    applyButtonPalette(style, "bluebutton", infoBase);
    applyButtonPalette(style, "yellowbutton", QColor("#f2d13b"));
    
    // Apply zoom overrides
    style.append(generateZoomOverrides(scale));
    
    app->setStyleSheet(style);
}

QString ThemeManager::generateZoomOverrides(double scale) const {
    return QString(
        "\n/* Zoom overrides */\n"
        "QWidget { font-size: %1px; }\n"
        "QLabel#EditorFileLabel { font-size: %2px; }\n"
        "QWidget#DockContent QLabel { font-size: %3px; }\n"
        "QWidget#DockContent QPushButton { font-size: %4px; padding: %5px %6px; }\n"
        "QTextEdit, QPlainTextEdit { font-size: %7px; }\n"
        "QLabel#EditorPlaceholder { font-size: %8px; }\n"
        "QPushButton { padding: %9px %10px; }\n"
        "QPushButton#RunAllButton { font-size: %11px; }\n"
        "QPushButton#AddCaseButton { font-size: %12px; }\n"
        "QDockWidget::title { padding-left: %13px; padding-top: %14px; padding-bottom: %15px; }\n"
        "QWidget#DockContent, QWidget#FileExplorer { padding-top: %16px; }\n"
        "QTreeView#FileExplorerTree { font-size: %17px; }\n"
        "QTreeView#FileExplorerTree::item { padding: %18px 0px; }\n"
        "QLabel#TestSummaryLabel { font-size: %19px; }\n"
        "QLabel#StressComplexityLabel { font-size: %20px; }\n"
        "QWidget#StressTestPanel QLabel { font-size: %21px; }\n"
        "QWidget#StressTestPanel QPushButton { font-size: %22px; }\n")
        .arg(scalePx(14, scale))   // 1  QWidget
        .arg(scalePx(13, scale))   // 2  EditorFileLabel
        .arg(scalePx(12, scale))   // 3  DockContent QLabel
        .arg(scalePx(12, scale))   // 4  DockContent QPushButton font
        .arg(scalePx(6, scale))    // 5  DockContent QPushButton padding-v
        .arg(scalePx(10, scale))   // 6  DockContent QPushButton padding-h
        .arg(scalePx(14, scale))   // 7  editors
        .arg(scalePx(14, scale))   // 8  EditorPlaceholder
        .arg(scalePx(8, scale))    // 9  QPushButton padding-v
        .arg(scalePx(12, scale))   // 10 QPushButton padding-h
        .arg(scalePx(18, scale))   // 11 RunAllButton
        .arg(scalePx(18, scale))   // 12 AddCaseButton
        .arg(scalePx(10, scale))   // 13 dock title padding-left
        .arg(scalePx(5, scale))    // 14 dock title padding-top
        .arg(scalePx(5, scale))    // 15 dock title padding-bottom
        .arg(scalePx(6, scale))    // 16 DockContent/FileExplorer padding-top
        .arg(scalePx(12, scale))   // 17 FileExplorerTree font
        .arg(scalePx(2, scale))    // 18 FileExplorerTree item padding
        .arg(scalePx(12, scale))   // 19 TestSummaryLabel
        .arg(scalePx(12, scale))   // 20 StressComplexityLabel
        .arg(scalePx(12, scale))   // 21 StressTestPanel QLabel
        .arg(scalePx(12, scale));  // 22 StressTestPanel QPushButton
}
