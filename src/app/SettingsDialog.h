#pragma once

#include <QWidget>

class QCheckBox;
class QTabWidget;
class QsciScintilla;
class QLineEdit;

class SettingsDialog : public QWidget {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);

    // Template settings
    void setTemplate(const QString &tmpl);
    QString getTemplate() const;

    // Compiler settings
    void setCompilerPath(const QString &path);
    QString compilerPath() const;
    
    void setCompilerFlags(const QString &flags);
    QString compilerFlags() const;

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

private slots:
    void onMultithreadingToggled(bool checked);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void setupUi();
    QWidget* createGeneralTab();
    QWidget* createTemplateTab();
    QWidget* createCompilerTab();
    QWidget* createExperimentalTab();
    QWidget* createAboutTab();

    QTabWidget *tabWidget_ = nullptr;
    
    // General tab
    QLineEdit *rootDirEdit_ = nullptr;
    
    // Template tab
    QsciScintilla *templateEditor_ = nullptr;
    
    // Compiler tab
    QLineEdit *compilerPathEdit_ = nullptr;
    QLineEdit *compilerFlagsEdit_ = nullptr;
    
    // Experimental tab
    QCheckBox *multithreadingCheckbox_ = nullptr;
};
