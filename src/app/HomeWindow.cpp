#include "app/HomeWindow.h"
#include "app/MainWindow.h"
#include "app/LandingButton.h"
#include "app/TrainerWindow.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QWidget>

HomeWindow::HomeWindow(QWidget *parent)
    : QMainWindow(parent) {
    themeManager_.apply(qApp, 1.0);
    setupUi();
}

void HomeWindow::setupUi() {
    resize(960, 560);
    setWindowTitle("CF Dojo");

    landingPage_ = new QWidget(this);
    landingPage_->setObjectName("LandingPage");
    landingPage_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QHBoxLayout(landingPage_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    trainingButton_ = new LandingButton("Training", ":/images/train.svg", landingPage_);
    trainingButton_->setObjectName("TrainingChoice");

    contestButton_ = new LandingButton("Contest", ":/images/battle.svg", landingPage_);
    contestButton_->setObjectName("ContestChoice");

    connect(trainingButton_, &LandingButton::clicked, this, [this]() {
        auto *window = new TrainerWindow();
        window->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(window, &QObject::destroyed, this, [this]() {
            show();
            raise();
            activateWindow();
        });
        window->show();
        window->raise();
        window->activateWindow();
        hide();
    });

    connect(contestButton_, &LandingButton::clicked, this, [this]() {
        auto *window = new MainWindow();
        window->setBaseWindowTitle("CF Dojo - Contest");
        window->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(window, &QObject::destroyed, this, [this]() {
            show();
            raise();
            activateWindow();
        });
        window->show();
        window->raise();
        window->activateWindow();
        hide();
    });

    layout->addWidget(trainingButton_, 1);
    layout->addWidget(contestButton_, 1);

    setCentralWidget(landingPage_);
}
