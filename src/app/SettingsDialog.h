#pragma once

#include <QWidget>
#include <QString>

class QCheckBox;
class QComboBox;
class QTabWidget;
class QLineEdit;

class SettingsDialog : public QWidget {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Template settings
    void setTemplate(const QString &tmpl);
    QString getTemplate() const;
    void setTranscludeTemplateEnabled(bool enabled);
    bool isTranscludeTemplateEnabled() const;

    // Language settings
    void setDefaultLanguage(const QString &language);
    QString defaultLanguage() const;

    void setCompilerPath(const QString &path); // C++ compiler
    QString compilerPath() const;
    void setCompilerFlags(const QString &flags);
    QString compilerFlags() const;

    void setPythonPath(const QString &path);
    QString pythonPath() const;
    void setPythonArgs(const QString &args);
    QString pythonArgs() const;

    void setJavaCompilerPath(const QString &path);
    QString javaCompilerPath() const;
    void setJavaRunPath(const QString &path);
    QString javaRunPath() const;
    void setJavaArgs(const QString &args);
    QString javaArgs() const;

    // General settings
    void setRootDir(const QString &path);
    QString rootDir() const;

    // Experimental settings
    void setMultithreadingEnabled(bool enabled);
    bool isMultithreadingEnabled() const;

signals:
    void settingsChanged();
    void saved();
    void cancelled();
    void closed();

private slots:
    void onMultithreadingToggled(bool checked);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    QWidget* createGeneralTab();
    QWidget* createTemplateTab();
    QWidget* createLanguagesTab();
    QWidget* createExperimentalTab();
    QWidget* createAboutTab();
    void openTemplateEditorDialog();
    void updateTemplateSummary();

    QTabWidget *tabWidget_ = nullptr;
    
    // General tab
    QLineEdit *rootDirEdit_ = nullptr;
    
    // Template tab
    QLineEdit *templateSummaryEdit_ = nullptr;
    QCheckBox *transcludeTemplateCheckbox_ = nullptr;
    QString templateText_;
    
    // Languages tab
    QComboBox *defaultLanguageCombo_ = nullptr;

    QLineEdit *cppCompilerPathEdit_ = nullptr;
    QLineEdit *cppCompilerFlagsEdit_ = nullptr;

    QLineEdit *pythonPathEdit_ = nullptr;
    QLineEdit *pythonArgsEdit_ = nullptr;

    QLineEdit *javaCompilerPathEdit_ = nullptr;
    QLineEdit *javaRunPathEdit_ = nullptr;
    QLineEdit *javaArgsEdit_ = nullptr;
    
    // Experimental tab
    QCheckBox *multithreadingCheckbox_ = nullptr;
};
