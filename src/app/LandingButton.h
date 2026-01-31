#pragma once

#include <QIcon>
#include <QPushButton>

class QLabel;

class LandingButton : public QPushButton {
    Q_OBJECT

public:
    explicit LandingButton(const QString &text, const QString &iconPath,
                           QWidget *parent = nullptr);

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateIcon();

    QIcon baseIcon_;
    QLabel *iconLabel_ = nullptr;
    QLabel *textLabel_ = nullptr;
    QColor cachedTint_;
};
