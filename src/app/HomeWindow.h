#pragma once

#include "theme/ThemeManager.h"

#include <QMainWindow>

class LandingButton;

class HomeWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit HomeWindow(QWidget *parent = nullptr);

private:
    void setupUi();

    ThemeManager themeManager_;
    QWidget *landingPage_ = nullptr;
    LandingButton *trainingButton_ = nullptr;
    LandingButton *contestButton_ = nullptr;
};
