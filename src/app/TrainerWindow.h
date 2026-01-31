#pragma once

#include "theme/ThemeManager.h"

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QVector>

class ActivityBarButton;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTreeWidget;
class QWidget;

struct DrillEntry {
    QString set;
    QString name;
    QStringList tags;
    int difficulty = 1;
    int minutes = 10;
    QString packPath;
};

class TrainerWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit TrainerWindow(QWidget *parent = nullptr);

private:
    void setupUi();
    void populateDrillList();
    void updateRecommendations();
    void updateSetLabel();
    void loadDefaultDrillSet();
    bool loadDrillSetFromFile(const QString &path);

    ThemeManager themeManager_;
    QWidget *activityBar_ = nullptr;
    ActivityBarButton *backButton_ = nullptr;
    QTreeWidget *drillList_ = nullptr;
    QComboBox *weaknessCombo_ = nullptr;
    QComboBox *goalCombo_ = nullptr;
    QComboBox *sessionCombo_ = nullptr;
    QLabel *setLabel_ = nullptr;
    QLabel *rulesSummaryLabel_ = nullptr;
    QListWidget *recommendationsList_ = nullptr;
    QPushButton *loadSetButton_ = nullptr;
    QVector<DrillEntry> drills_;
    QString drillSetName_;
    QString drillSetPath_;
};
