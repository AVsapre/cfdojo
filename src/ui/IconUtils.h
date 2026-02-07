#pragma once

#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QSize>

namespace IconUtils {

inline QIcon makeTintedIcon(const QString &path, const QColor &color, const QSize &size,
                            qreal dpr = 0.0) {
    if (dpr <= 0.0) {
        dpr = qApp ? qApp->devicePixelRatio() : 1.0;
    }
    const QSize pixmapSize(static_cast<int>(size.width() * dpr),
                           static_cast<int>(size.height() * dpr));
    QPixmap base = QIcon(path).pixmap(pixmapSize);
    if (base.isNull()) {
        return QIcon();
    }
    base.setDevicePixelRatio(dpr);

    QPixmap tinted(base.size());
    tinted.setDevicePixelRatio(dpr);
    tinted.fill(Qt::transparent);

    QPainter painter(&tinted);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawPixmap(0, 0, base);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);

    return QIcon(tinted);
}

} // namespace IconUtils
