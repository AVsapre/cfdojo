#include "app/TrainerWindow.h"

#include "app/ActivityBarButton.h"

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QSizePolicy>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cmath>

namespace {
constexpr int kActivityBarWidth = 50;

struct DrillEntry {
    QString set;
    QString name;
    QStringList tags;
    int difficulty = 1;
    int minutes = 10;
};

const QVector<DrillEntry> &drillCatalog() {
    static const QVector<DrillEntry> catalog = {
        {"Accuracy", "Edge Case Blitz", {"accuracy", "edge cases", "debugging"}, 2, 20},
        {"Accuracy", "Off-by-One Gauntlet", {"accuracy", "implementation"}, 2, 25},
        {"Accuracy", "Sample Sleuth", {"accuracy", "testing"}, 1, 15},
        {"Speed", "Warmup Ladder", {"speed", "fundamentals"}, 1, 15},
        {"Speed", "Two-Minute Steps", {"speed", "math"}, 2, 20},
        {"Speed", "Template Sprint", {"speed", "implementation"}, 2, 25},
        {"Core Algorithms", "Two Pointers Run", {"arrays", "two pointers", "speed"}, 2, 30},
        {"Core Algorithms", "Binary Search Range", {"binary search", "accuracy"}, 2, 25},
        {"Core Algorithms", "Greedy Choice Drill", {"greedy", "reasoning"}, 3, 35},
        {"Core Algorithms", "DP State Sketch", {"dp", "reasoning"}, 3, 40},
        {"Core Algorithms", "String Prefix Play", {"strings", "prefix", "implementation"}, 2, 30},
        {"Core Algorithms", "Graph BFS/DFS Circuit", {"graphs", "implementation"}, 3, 35},
        {"Implementation", "Input Parsing Clinic", {"implementation", "accuracy"}, 2, 20},
        {"Implementation", "Invariant Checks", {"debugging", "accuracy"}, 3, 30},
        {"Implementation", "Complexity Triage", {"optimization", "speed"}, 3, 25},
        {"Advanced", "Segment Tree Mechanics", {"data structures", "implementation", "advanced"}, 4, 45},
        {"Advanced", "Dijkstra Precision", {"graphs", "accuracy", "advanced"}, 4, 45},
        {"Advanced", "Bitmask DP Mini", {"dp", "advanced"}, 4, 50},
    };
    return catalog;
}

struct Recommendation {
    DrillEntry drill;
    int score = 0;
    QStringList reasons;
};

QVector<Recommendation> recommendDrills(const QString &weakness,
                                        const QString &goal,
                                        const QString &session) {
    QVector<Recommendation> results;
    results.reserve(drillCatalog().size());

    for (const auto &drill : drillCatalog()) {
        Recommendation rec;
        rec.drill = drill;

        const auto addTagBoost = [&](const QString &tag, int weight, const QString &reason) {
            if (drill.tags.contains(tag, Qt::CaseInsensitive)) {
                rec.score += weight;
                rec.reasons << reason;
            }
        };

        if (weakness == "Accuracy") {
            addTagBoost("accuracy", 4, "accuracy");
            addTagBoost("edge cases", 3, "edge cases");
            addTagBoost("debugging", 2, "debugging");
        } else if (weakness == "Speed") {
            addTagBoost("speed", 4, "speed");
            addTagBoost("fundamentals", 2, "fundamentals");
        } else if (weakness == "Implementation") {
            addTagBoost("implementation", 4, "implementation");
            addTagBoost("debugging", 2, "debugging");
        } else if (weakness == "DP") {
            addTagBoost("dp", 4, "dp");
        } else if (weakness == "Graphs") {
            addTagBoost("graphs", 4, "graphs");
        } else if (weakness == "Greedy") {
            addTagBoost("greedy", 4, "greedy");
        } else if (weakness == "Math") {
            addTagBoost("math", 4, "math");
        } else if (weakness == "Strings") {
            addTagBoost("strings", 4, "strings");
        }

        if (goal == "Fundamentals") {
            if (drill.difficulty <= 2) {
                rec.score += 2;
                rec.reasons << "fundamentals";
            } else if (drill.difficulty >= 4) {
                rec.score -= 1;
            }
        } else if (goal == "Advanced") {
            if (drill.difficulty >= 4) {
                rec.score += 2;
                rec.reasons << "advanced";
            } else if (drill.difficulty <= 2) {
                rec.score -= 1;
            }
        } else if (goal == "Variety") {
            if (drill.tags.size() >= 2) {
                rec.score += 1;
                rec.reasons << "variety";
            }
        }

        if (session == "Quick (15-25 min)") {
            if (drill.minutes <= 25) {
                rec.score += 2;
                rec.reasons << "short session";
            } else if (drill.minutes >= 45) {
                rec.score -= 1;
            }
        } else if (session == "Medium (30-45 min)") {
            if (drill.minutes >= 30 && drill.minutes <= 45) {
                rec.score += 2;
                rec.reasons << "medium session";
            }
        } else if (session == "Deep (60+ min)") {
            if (drill.minutes >= 50) {
                rec.score += 2;
                rec.reasons << "deep session";
            } else if (drill.minutes <= 25) {
                rec.score -= 1;
            }
        }

        const int distanceFromMid = std::abs(3 - drill.difficulty);
        rec.score += std::max(0, 2 - distanceFromMid);

        results.push_back(rec);
    }

    std::sort(results.begin(), results.end(),
              [](const Recommendation &a, const Recommendation &b) {
                  if (a.score != b.score) {
                      return a.score > b.score;
                  }
                  if (a.drill.set != b.drill.set) {
                      return a.drill.set < b.drill.set;
                  }
                  return a.drill.name < b.drill.name;
              });

    return results;
}
}

TrainerWindow::TrainerWindow(QWidget *parent)
    : QMainWindow(parent) {
    themeManager_.apply(qApp, 1.0);
    setupUi();
}

void TrainerWindow::setupUi() {
    resize(1200, 800);
    setWindowTitle("CF Dojo - Training");

    auto *central = new QWidget(this);
    auto *layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    activityBar_ = new QWidget(central);
    activityBar_->setObjectName("ActivityBar");
    activityBar_->setFixedWidth(kActivityBarWidth);
    activityBar_->setAttribute(Qt::WA_StyledBackground, true);

    auto *barLayout = new QHBoxLayout(activityBar_);
    barLayout->setContentsMargins(0, 0, 0, 0);
    barLayout->setSpacing(0);

    auto *buttonColumn = new QWidget(activityBar_);
    buttonColumn->setObjectName("ActivityBarButtons");
    auto *buttonLayout = new QVBoxLayout(buttonColumn);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);

    buttonLayout->addStretch();

    backButton_ = new ActivityBarButton(":/images/arrow-left.svg", buttonColumn);
    backButton_->setObjectName("BackButton");
    backButton_->setFixedHeight(kActivityBarWidth);
    backButton_->setToolTip("Back");
    backButton_->setTintColors(themeManager_.textColor(),
                               themeManager_.textColor(),
                               QColor("#808080"));

    connect(backButton_, &QPushButton::clicked, this, [this]() {
        close();
    });

    buttonLayout->addWidget(backButton_, 0, Qt::AlignBottom);

    barLayout->addWidget(buttonColumn);

    auto *edgeLine = new QWidget(activityBar_);
    edgeLine->setObjectName("ActivityBarEdge");
    edgeLine->setFixedWidth(1);
    edgeLine->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    barLayout->addWidget(edgeLine);

    layout->addWidget(activityBar_);

    auto *content = new QWidget(central);
    content->setObjectName("TrainerContent");
    content->setAttribute(Qt::WA_StyledBackground, true);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(16, 16, 16, 16);
    contentLayout->setSpacing(12);

    auto *listTitle = new QLabel("List of drills", content);
    listTitle->setObjectName("PanelTitle");
    contentLayout->addWidget(listTitle);

    drillList_ = new QTreeWidget(content);
    drillList_->setObjectName("DrillList");
    drillList_->setColumnCount(4);
    drillList_->setHeaderLabels({"Drill", "Focus", "Level", "Time"});
    drillList_->setRootIsDecorated(true);
    drillList_->setIndentation(14);
    drillList_->setSelectionMode(QAbstractItemView::NoSelection);
    drillList_->setAlternatingRowColors(true);
    drillList_->header()->setStretchLastSection(false);
    drillList_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    drillList_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    drillList_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    drillList_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    contentLayout->addWidget(drillList_, 1);

    auto *focusTitle = new QLabel("Focus", content);
    focusTitle->setObjectName("PanelTitle");
    contentLayout->addWidget(focusTitle);

    auto *focusPanel = new QWidget(content);
    auto *focusLayout = new QFormLayout(focusPanel);
    focusLayout->setContentsMargins(0, 0, 0, 0);
    focusLayout->setSpacing(8);
    focusLayout->setLabelAlignment(Qt::AlignLeft);

    weaknessCombo_ = new QComboBox(focusPanel);
    weaknessCombo_->addItems({"Balanced", "Accuracy", "Speed", "Implementation", "DP",
                              "Graphs", "Greedy", "Math", "Strings"});
    goalCombo_ = new QComboBox(focusPanel);
    goalCombo_->addItems({"Fundamentals", "Advanced", "Variety"});
    sessionCombo_ = new QComboBox(focusPanel);
    sessionCombo_->addItems({"Quick (15-25 min)", "Medium (30-45 min)", "Deep (60+ min)"});

    focusLayout->addRow("Weakness:", weaknessCombo_);
    focusLayout->addRow("Goal:", goalCombo_);
    focusLayout->addRow("Session:", sessionCombo_);
    contentLayout->addWidget(focusPanel);

    rulesSummaryLabel_ = new QLabel(content);
    rulesSummaryLabel_->setWordWrap(true);
    contentLayout->addWidget(rulesSummaryLabel_);

    auto *recommendTitle = new QLabel("Recommendations", content);
    recommendTitle->setObjectName("PanelTitle");
    contentLayout->addWidget(recommendTitle);

    recommendationsList_ = new QListWidget(content);
    recommendationsList_->setObjectName("RecommendationsList");
    recommendationsList_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    contentLayout->addWidget(recommendationsList_, 1);

    populateDrillList();

    connect(weaknessCombo_, &QComboBox::currentTextChanged, this, [this]() {
        updateRecommendations();
    });
    connect(goalCombo_, &QComboBox::currentTextChanged, this, [this]() {
        updateRecommendations();
    });
    connect(sessionCombo_, &QComboBox::currentTextChanged, this, [this]() {
        updateRecommendations();
    });

    updateRecommendations();

    layout->addWidget(content, 1);

    setCentralWidget(central);
}

void TrainerWindow::populateDrillList() {
    if (!drillList_) {
        return;
    }

    drillList_->clear();

    QHash<QString, QTreeWidgetItem *> setItems;
    QStringList setOrder;

    for (const auto &drill : drillCatalog()) {
        if (!setItems.contains(drill.set)) {
            auto *setItem = new QTreeWidgetItem(drillList_);
            setItem->setText(0, drill.set + " Set");
            setItem->setFirstColumnSpanned(true);
            QFont font = setItem->font(0);
            font.setBold(true);
            setItem->setFont(0, font);
            setItems.insert(drill.set, setItem);
            setOrder.append(drill.set);
        }

        auto *child = new QTreeWidgetItem(setItems.value(drill.set));
        child->setText(0, drill.name);
        child->setText(1, drill.tags.join(", "));
        child->setText(2, QString("L%1").arg(drill.difficulty));
        child->setText(3, QString("%1 min").arg(drill.minutes));
    }

    for (const auto &setName : setOrder) {
        if (auto *item = setItems.value(setName)) {
            item->setExpanded(true);
        }
    }
}

void TrainerWindow::updateRecommendations() {
    if (!recommendationsList_ || !weaknessCombo_ || !goalCombo_ || !sessionCombo_) {
        return;
    }

    const QString weakness = weaknessCombo_->currentText();
    const QString goal = goalCombo_->currentText();
    const QString session = sessionCombo_->currentText();

    recommendationsList_->clear();

    const auto recommendations = recommendDrills(weakness, goal, session);
    int shown = 0;
    for (const auto &rec : recommendations) {
        if (shown >= 6) {
            break;
        }
        QString itemText = QString("%1 — %2 • L%3 • %4 min")
                               .arg(rec.drill.name)
                               .arg(rec.drill.tags.join(", "))
                               .arg(rec.drill.difficulty)
                               .arg(rec.drill.minutes);
        if (!rec.reasons.isEmpty()) {
            itemText += QString(" (matched: %1)").arg(rec.reasons.join(", "));
        }
        recommendationsList_->addItem(itemText);
        ++shown;
    }

    if (shown == 0) {
        recommendationsList_->addItem("No drills available.");
    }

    if (rulesSummaryLabel_) {
        QStringList summary;
        if (weakness == "Accuracy") {
            summary << "Accuracy → boost accuracy/edge cases/debugging";
        } else if (weakness == "Speed") {
            summary << "Speed → boost speed/fundamentals";
        } else if (weakness == "Implementation") {
            summary << "Implementation → boost implementation/debugging";
        } else if (weakness == "DP") {
            summary << "DP → boost dp drills";
        } else if (weakness == "Graphs") {
            summary << "Graphs → boost graph drills";
        } else if (weakness == "Greedy") {
            summary << "Greedy → boost greedy drills";
        } else if (weakness == "Math") {
            summary << "Math → boost math drills";
        } else if (weakness == "Strings") {
            summary << "Strings → boost string drills";
        } else {
            summary << "Balanced → no weakness boost";
        }

        if (goal == "Fundamentals") {
            summary << "Fundamentals → prefer L1-L2";
        } else if (goal == "Advanced") {
            summary << "Advanced → prefer L4+";
        } else {
            summary << "Variety → prefer multi-skill drills";
        }

        if (session == "Quick (15-25 min)") {
            summary << "Quick → ≤25 min";
        } else if (session == "Medium (30-45 min)") {
            summary << "Medium → 30-45 min";
        } else {
            summary << "Deep → 50+ min";
        }

        rulesSummaryLabel_->setText(QString("Rules applied: %1").arg(summary.join(" • ")));
    }
}
