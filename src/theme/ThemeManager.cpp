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

    const QColor bgHover = lightenColor(colors_.background, 10);
    const QColor bgPressed = darkenColor(colors_.background, 8);

    const QColor primaryHover = lightenColor(primaryBase, 10);
    const QColor primaryPressed = darkenColor(primaryBase, 12);

    const QColor successHover = lightenColor(successBase, 10);
    const QColor successPressed = darkenColor(successBase, 12);

    const QColor dangerHover = lightenColor(dangerBase, 10);
    const QColor dangerPressed = darkenColor(dangerBase, 12);

    const QColor infoHover = lightenColor(infoBase, 10);
    const QColor infoPressed = darkenColor(infoBase, 12);

    style.replace("@bgHover@", bgHover.name());
    style.replace("@bgPressed@", bgPressed.name());

    style.replace("@primary@", primaryBase.name());
    style.replace("@primaryHover@", primaryHover.name());
    style.replace("@primaryPressed@", primaryPressed.name());

    style.replace("@success@", successBase.name());
    style.replace("@successHover@", successHover.name());
    style.replace("@successPressed@", successPressed.name());

    style.replace("@danger@", dangerBase.name());
    style.replace("@dangerHover@", dangerHover.name());
    style.replace("@dangerPressed@", dangerPressed.name());

    style.replace("@info@", infoBase.name());
    style.replace("@infoHover@", infoHover.name());
    style.replace("@infoPressed@", infoPressed.name());
    
    // Apply zoom overrides
    style.append(generateZoomOverrides(scale));
    
    app->setStyleSheet(style);
}

QString ThemeManager::generateZoomOverrides(double scale) const {
    return QString(
        "\n/* Zoom overrides */\n"
        "QWidget { font-size: %1px; }\n"
        "QWidget#DockContent QLabel { font-size: %2px; }\n"
        "QWidget#DockContent QPushButton { font-size: %3px; padding: %4px %5px; }\n"
        "QTextEdit, QPlainTextEdit { font-size: %6px; }\n"
        "QPushButton { padding: %7px %8px; }\n"
        "QDockWidget::title { padding-left: %9px; padding-top: %10px; padding-bottom: %11px; }\n"
        "QWidget#DockContent, QWidget#FileExplorer { padding-top: %12px; }\n"
        "QTreeView#FileExplorerTree { font-size: %13px; }\n"
        "QTreeView#FileExplorerTree::item { padding: %14px 0px; }\n")
        .arg(scalePx(14, scale))
        .arg(scalePx(12, scale))
        .arg(scalePx(12, scale))
        .arg(scalePx(6, scale))
        .arg(scalePx(10, scale))
        .arg(scalePx(14, scale))
        .arg(scalePx(8, scale))
        .arg(scalePx(12, scale))
        .arg(scalePx(10, scale))
        .arg(scalePx(5, scale))
        .arg(scalePx(5, scale))
        .arg(scalePx(6, scale))
        .arg(scalePx(12, scale))
        .arg(scalePx(2, scale));
}
