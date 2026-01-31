#include "app/LandingButton.h"

#include <QEvent>
#include <QLabel>
#include <QPainter>
#include <QPalette>
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
    : QPushButton(parent), baseIcon_(iconPath) {
    setAttribute(Qt::WA_StyledBackground, true);
    setAttribute(Qt::WA_Hover, true);
    setMouseTracking(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(14);

    layout->addStretch(1);

    iconLabel_ = new QLabel(this);
    iconLabel_->setAlignment(Qt::AlignCenter);
    iconLabel_->setFixedSize(kIconSize, kIconSize);
    iconLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    layout->addWidget(iconLabel_, 0, Qt::AlignCenter);

    textLabel_ = new QLabel(text, this);
    textLabel_->setObjectName("LandingButtonText");
    textLabel_->setAlignment(Qt::AlignCenter);
    textLabel_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    layout->addWidget(textLabel_, 0, Qt::AlignCenter);

    layout->addStretch(1);
    updateIcon();
}

void LandingButton::changeEvent(QEvent *event) {
    QPushButton::changeEvent(event);
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::StyleChange) {
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
    if (iconLabel_) {
        iconLabel_->setPixmap(tintPixmap(base, color));
    }
}
