#pragma once

#include <QIcon>
#include <QWidget>

class QLabel;

class LandingButton : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool hovered READ isHovered)
    Q_PROPERTY(bool pressed READ isPressed)

public:
    explicit LandingButton(const QString &text, const QString &iconPath,
                           QWidget *parent = nullptr);

    bool isHovered() const { return hovered_; }
    bool isPressed() const { return pressed_; }

signals:
    void clicked();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updateIcon();

    QIcon baseIcon_;
    QLabel *iconLabel_;
    QLabel *textLabel_;
    QColor cachedTint_;
    bool hovered_ = false;
    bool pressed_ = false;
};
