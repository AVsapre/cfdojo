#include "app/ActivityBarButton.h"

#include <QEnterEvent>
#include <QIcon>
#include <QPainter>
#include <QResizeEvent>
#include <cmath>

namespace {
constexpr int kActivityIconSize = 32;
}

ActivityBarButton::ActivityBarButton(const QString &iconPath, QWidget *parent)
    : QPushButton(parent),
      iconPath_(iconPath),
      normalColor_("#d4d4d4"),
      hoveredColor_("#d4d4d4"),
      inactiveColor_("#808080") {
    setFocusPolicy(Qt::NoFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    updateIcon();
}

void ActivityBarButton::setActiveState(bool active) {
    if (active_ != active) {
        active_ = active;
    }
    updateIcon();
}

void ActivityBarButton::setTintColors(const QColor &normal, const QColor &hovered, 
                                       const QColor &inactive) {
    normalColor_ = normal;
    hoveredColor_ = hovered;
    inactiveColor_ = inactive;
    updateIcon();
}

void ActivityBarButton::enterEvent(QEnterEvent *event) {
    hovered_ = true;
    updateIcon();
    QPushButton::enterEvent(event);
}

void ActivityBarButton::leaveEvent(QEvent *event) {
    hovered_ = false;
    updateIcon();
    QPushButton::leaveEvent(event);
}

void ActivityBarButton::resizeEvent(QResizeEvent *event) {
    QPushButton::resizeEvent(event);
    updateIcon();
}

void ActivityBarButton::setScale(double scale) {
    scale_ = scale;
    updateIcon();
}

void ActivityBarButton::updateIcon() {
    const int targetIconSize = static_cast<int>(std::round(kActivityIconSize * scale_));
    const qreal dpr = devicePixelRatioF();

    const QSize pixmapSize(static_cast<int>(targetIconSize * dpr),
                           static_cast<int>(targetIconSize * dpr));

    QPixmap basePixmap = QIcon(iconPath_).pixmap(pixmapSize);
    if (basePixmap.isNull()) {
        basePixmap = QPixmap(iconPath_);
        if (!basePixmap.isNull() && basePixmap.size() != pixmapSize) {
            basePixmap = basePixmap.scaled(
                pixmapSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    if (basePixmap.isNull()) {
        return;
    }
    basePixmap.setDevicePixelRatio(dpr);

    QColor tintColor;
    if (hovered_ || active_) {
        tintColor = hoveredColor_;  // White when hovered or active
    } else {
        tintColor = inactiveColor_; // Gray when inactive
    }

    setIcon(QIcon(tintPixmap(basePixmap, tintColor)));
    setIconSize(QSize(targetIconSize, targetIconSize));

    cachedIconSize_ = targetIconSize;
    cachedDpr_ = dpr;
}

QPixmap ActivityBarButton::tintPixmap(const QPixmap &source, const QColor &color) const {
    if (source.isNull()) {
        return source;
    }

    QPixmap tinted(source.size());
    tinted.setDevicePixelRatio(source.devicePixelRatio());
    tinted.fill(Qt::transparent);

    QPainter painter(&tinted);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    return tinted;
}
