#ifndef SETTINGSPAGE_H
#define SETTINGSPAGE_H

#include "appsettings.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class SettingsPage : public QWidget
{
    Q_OBJECT

public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setSettings(const AppSettings &settings);
    AppSettings settings() const;

signals:
    void settingsApplied(const AppSettings &settings);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void chooseCsvDirectory();
    void applySettings();
    void updateBackendHint();

private:
    void retranslateUi();

    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageSubtitle = nullptr;
    QGroupBox *m_connectionGroup = nullptr;
    QGroupBox *m_recordingGroup = nullptr;
    QLabel *m_hostLabel = nullptr;
    QLabel *m_portLabel = nullptr;
    QLabel *m_clientIdLabel = nullptr;
    QLabel *m_topicPrefixLabel = nullptr;
    QLabel *m_usernameLabel = nullptr;
    QLabel *m_passwordLabel = nullptr;
    QLabel *m_csvDirectoryLabel = nullptr;
    QLabel *m_languageLabel = nullptr;
    QLabel *m_backendHint = nullptr;
    QLabel *m_passwordHint = nullptr;
    QLabel *m_restartHint = nullptr;

    QLineEdit *m_hostEdit = nullptr;
    QSpinBox *m_portSpin = nullptr;
    QLineEdit *m_clientIdEdit = nullptr;
    QLineEdit *m_topicPrefixEdit = nullptr;
    QLineEdit *m_usernameEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QCheckBox *m_autoConnectCheck = nullptr;
    QCheckBox *m_demoModeCheck = nullptr;
    QCheckBox *m_csvEnabledCheck = nullptr;
    QLineEdit *m_csvDirectoryEdit = nullptr;
    QPushButton *m_browseButton = nullptr;
    QComboBox *m_languageCombo = nullptr;
    QPushButton *m_applyButton = nullptr;
};

#endif
