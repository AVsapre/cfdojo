#include "app/LandingButton.h"

#include <QEnterEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QVBoxLayout>

namespace {
constexpr int kIconSize = 128;

QPixmap tintPixmap(const QPixmap &source, const QColor &color) {
    if (source.isNull()) {
        return source;
    }
    QPixmap tinted(source.size());
    tinted.setDevicePixelRatio(source.devicePixelRatio());
    tinted.fill(Qt::transparent);
    QPainter painter(&tinted);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    return tinted;
}
} // namespace

LandingButton::LandingButton(const QString &text, const QString &iconPath,
                             QWidget *parent)
    : QWidget(parent), baseIcon_(iconPath) {
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    // Spacer pushes content to center
    layout->addStretch(1);

    iconLabel_ = new QLabel(this);
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setFixedSize(kIconSize, kIconSize);
    layout->addWidget(iconLabel_, 0, Qt::AlignCenter);

    textLabel_ = new QLabel(text, this);
    textLabel_->setObjectName("LandingButtonText");
    textLabel_->setAlignment(Qt::AlignCenter);
    layout->addWidget(textLabel_, 0, Qt::AlignCenter);

    // Spacer pushes content to center
    layout->addStretch(1);

    updateIcon();
}

void LandingButton::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        pressed_ = true;
        style()->polish(this);
        update();
    }
    QWidget::mousePressEvent(event);
}

void LandingButton::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && pressed_) {
        pressed_ = false;
        style()->polish(this);
        update();
        if (rect().contains(event->pos())) {
            emit clicked();
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void LandingButton::enterEvent(QEnterEvent *event) {
    hovered_ = true;
    style()->polish(this);
    update();
    QWidget::enterEvent(event);
}

void LandingButton::leaveEvent(QEvent *event) {
    hovered_ = false;
    pressed_ = false;
    style()->polish(this);
    update();
    QWidget::leaveEvent(event);
}

void LandingButton::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange) {
        cachedTint_ = QColor();  // Force refresh
        updateIcon();
    }
}

void LandingButton::updateIcon() {
    const QColor color = palette().color(QPalette::ButtonText);
    if (color == cachedTint_) {
        return;
    }
    cachedTint_ = color;

    const qreal dpr = devicePixelRatioF();
    QPixmap base = baseIcon_.pixmap(QSize(kIconSize, kIconSize) * dpr);
    base.setDevicePixelRatio(dpr);
    iconLabel_->setPixmap(tintPixmap(base, color));
}
