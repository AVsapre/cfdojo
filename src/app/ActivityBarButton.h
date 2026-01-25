#pragma once

#include <QPushButton>
#include <QColor>
#include <QPixmap>

class ActivityBarButton : public QPushButton {
    Q_OBJECT

public:
    explicit ActivityBarButton(const QString &iconPath, QWidget *parent = nullptr);

    void setActiveState(bool active);
    void setTintColors(const QColor &normal, const QColor &hovered, const QColor &inactive);

protected:
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateIcon();
    QPixmap tintPixmap(const QPixmap &source, const QColor &color) const;

    QString iconPath_;
    bool hovered_ = false;
    bool active_ = false;  // Start inactive (gray); becomes white on hover
    QColor normalColor_;
    QColor hoveredColor_;
    QColor inactiveColor_;
    int cachedIconSize_ = 0;
    qreal cachedDpr_ = 0.0;
};
