#include "theme/ThemeManager.h"

#include <QApplication>
#include <QFile>
#include <QString>
#include <QStyleFactory>
#include <algorithm>
#include <cmath>

namespace {
int scalePx(int value, double scale) {
    return std::max(1, static_cast<int>(std::round(value * scale)));
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
        "QWidget#DockContent { padding-top: %12px; }\n")
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
        .arg(scalePx(6, scale));
}
