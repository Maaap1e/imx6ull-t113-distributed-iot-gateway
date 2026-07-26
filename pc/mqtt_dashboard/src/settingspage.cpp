#include "settingspage.h"

#include "mqtttransport.h"

#include <QCheckBox>
#include <QComboBox>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(16);

    m_pageTitle = new QLabel(this);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    m_pageSubtitle = new QLabel(this);
    m_pageSubtitle->setObjectName(QStringLiteral("pageSubtitle"));
    root->addWidget(m_pageTitle);
    root->addWidget(m_pageSubtitle);

    m_connectionGroup = new QGroupBox(this);
    m_connectionGroup->setObjectName(QStringLiteral("settingsGroup"));
    auto *connectionLayout = new QFormLayout(m_connectionGroup);
    connectionLayout->setContentsMargins(22, 22, 22, 22);
    connectionLayout->setHorizontalSpacing(22);
    connectionLayout->setVerticalSpacing(12);

    m_hostEdit = new QLineEdit(m_connectionGroup);
    m_portSpin = new QSpinBox(m_connectionGroup);
    m_portSpin->setRange(1, 65535);
    m_clientIdEdit = new QLineEdit(m_connectionGroup);
    m_topicPrefixEdit = new QLineEdit(m_connectionGroup);
    m_usernameEdit = new QLineEdit(m_connectionGroup);
    m_passwordEdit = new QLineEdit(m_connectionGroup);
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_autoConnectCheck = new QCheckBox(m_connectionGroup);
    m_demoModeCheck = new QCheckBox(m_connectionGroup);
    m_backendHint = new QLabel(m_connectionGroup);
    m_backendHint->setObjectName(QStringLiteral("mutedLabel"));
    m_backendHint->setWordWrap(true);
    m_passwordHint = new QLabel(m_connectionGroup);
    m_passwordHint->setObjectName(QStringLiteral("mutedLabel"));
    m_passwordHint->setWordWrap(true);

    m_hostLabel = new QLabel(m_connectionGroup);
    m_portLabel = new QLabel(m_connectionGroup);
    m_clientIdLabel = new QLabel(m_connectionGroup);
    m_topicPrefixLabel = new QLabel(m_connectionGroup);
    m_usernameLabel = new QLabel(m_connectionGroup);
    m_passwordLabel = new QLabel(m_connectionGroup);
    connectionLayout->addRow(m_hostLabel, m_hostEdit);
    connectionLayout->addRow(m_portLabel, m_portSpin);
    connectionLayout->addRow(m_clientIdLabel, m_clientIdEdit);
    connectionLayout->addRow(m_topicPrefixLabel, m_topicPrefixEdit);
    connectionLayout->addRow(m_usernameLabel, m_usernameEdit);
    connectionLayout->addRow(m_passwordLabel, m_passwordEdit);
    connectionLayout->addRow(QString(), m_passwordHint);
    connectionLayout->addRow(QString(), m_autoConnectCheck);
    connectionLayout->addRow(QString(), m_demoModeCheck);
    connectionLayout->addRow(QString(), m_backendHint);
    root->addWidget(m_connectionGroup);

    m_recordingGroup = new QGroupBox(this);
    m_recordingGroup->setObjectName(QStringLiteral("settingsGroup"));
    auto *recordingLayout = new QFormLayout(m_recordingGroup);
    recordingLayout->setContentsMargins(22, 22, 22, 22);
    recordingLayout->setHorizontalSpacing(22);
    recordingLayout->setVerticalSpacing(12);

    m_csvEnabledCheck = new QCheckBox(m_recordingGroup);
    m_csvDirectoryEdit = new QLineEdit(m_recordingGroup);
    m_browseButton = new QPushButton(m_recordingGroup);
    auto *directoryRow = new QHBoxLayout;
    directoryRow->addWidget(m_csvDirectoryEdit, 1);
    directoryRow->addWidget(m_browseButton);
    m_languageCombo = new QComboBox(m_recordingGroup);
    m_languageCombo->addItem(QStringLiteral("中文"), QStringLiteral("zh_CN"));
    m_languageCombo->addItem(QStringLiteral("English"),
                             QStringLiteral("en_US"));
    m_restartHint = new QLabel(m_recordingGroup);
    m_restartHint->setObjectName(QStringLiteral("mutedLabel"));
    m_restartHint->setWordWrap(true);

    m_csvDirectoryLabel = new QLabel(m_recordingGroup);
    m_languageLabel = new QLabel(m_recordingGroup);
    recordingLayout->addRow(QString(), m_csvEnabledCheck);
    recordingLayout->addRow(m_csvDirectoryLabel, directoryRow);
    recordingLayout->addRow(m_languageLabel, m_languageCombo);
    recordingLayout->addRow(QString(), m_restartHint);
    root->addWidget(m_recordingGroup);

    m_applyButton = new QPushButton(this);
    m_applyButton->setObjectName(QStringLiteral("primaryButton"));
    auto *applyRow = new QHBoxLayout;
    applyRow->addStretch();
    applyRow->addWidget(m_applyButton);
    root->addLayout(applyRow);
    root->addStretch();

    connect(m_browseButton, &QPushButton::clicked,
            this, &SettingsPage::chooseCsvDirectory);
    connect(m_applyButton, &QPushButton::clicked,
            this, &SettingsPage::applySettings);
    connect(m_demoModeCheck, &QCheckBox::toggled,
            this, &SettingsPage::updateBackendHint);

    retranslateUi();
    updateBackendHint();
}

void SettingsPage::setSettings(const AppSettings &settings)
{
    m_hostEdit->setText(settings.brokerHost);
    m_portSpin->setValue(settings.brokerPort);
    m_clientIdEdit->setText(settings.clientId);
    m_topicPrefixEdit->setText(settings.topicPrefix);
    m_usernameEdit->setText(settings.username);
    m_passwordEdit->setText(settings.password);
    m_autoConnectCheck->setChecked(settings.autoConnect);
    m_demoModeCheck->setChecked(settings.demoMode);
    m_csvEnabledCheck->setChecked(settings.csvEnabled);
    m_csvDirectoryEdit->setText(settings.csvDirectory);
    const int languageIndex =
        m_languageCombo->findData(settings.language);
    m_languageCombo->setCurrentIndex(languageIndex >= 0 ? languageIndex : 0);
    updateBackendHint();
}

AppSettings SettingsPage::settings() const
{
    AppSettings result;
    result.brokerHost = m_hostEdit->text().trimmed();
    result.brokerPort = static_cast<quint16>(m_portSpin->value());
    result.clientId = m_clientIdEdit->text().trimmed();
    result.topicPrefix = m_topicPrefixEdit->text().trimmed();
    result.username = m_usernameEdit->text().trimmed();
    result.password = m_passwordEdit->text();
    result.autoConnect = m_autoConnectCheck->isChecked();
    result.demoMode = m_demoModeCheck->isChecked();
    result.csvEnabled = m_csvEnabledCheck->isChecked();
    result.csvDirectory = m_csvDirectoryEdit->text().trimmed();
    result.language = m_languageCombo->currentData().toString();
    return result;
}

void SettingsPage::chooseCsvDirectory()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, tr("Select CSV directory"), m_csvDirectoryEdit->text());
    if (!directory.isEmpty()) {
        m_csvDirectoryEdit->setText(directory);
    }
}

void SettingsPage::applySettings()
{
    const AppSettings value = settings();
    if (value.brokerHost.isEmpty() || value.clientId.isEmpty() ||
        value.topicPrefix.isEmpty()) {
        m_backendHint->setText(
            tr("Host, Client ID and topic prefix are required."));
        return;
    }
    emit settingsApplied(value);
}

void SettingsPage::updateBackendHint()
{
    if (m_demoModeCheck->isChecked()) {
        m_backendHint->setText(
            tr("Demo mode generates local data and does not contact a Broker."));
    } else if (pahoBackendCompiled()) {
        m_backendHint->setText(
            tr("Paho MQTT C backend is available in this build."));
    } else {
        m_backendHint->setText(
            tr("Paho backend is not built. Enable Demo mode or rebuild with "
               "a Windows Paho development package."));
    }
}

void SettingsPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void SettingsPage::retranslateUi()
{
    m_pageTitle->setText(tr("Application Settings"));
    m_pageSubtitle->setText(
        tr("Broker, topic, recording and language preferences"));
    m_connectionGroup->setTitle(tr("MQTT Connection"));
    m_recordingGroup->setTitle(tr("Recording and Interface"));
    m_hostLabel->setText(tr("Broker host"));
    m_portLabel->setText(tr("TCP port"));
    m_clientIdLabel->setText(tr("Client ID"));
    m_topicPrefixLabel->setText(tr("Topic prefix"));
    m_usernameLabel->setText(tr("Username"));
    m_passwordLabel->setText(tr("Password"));
    m_csvDirectoryLabel->setText(tr("CSV directory"));
    m_languageLabel->setText(tr("Interface language"));
    m_autoConnectCheck->setText(tr("Connect automatically at startup"));
    m_demoModeCheck->setText(tr("Use built-in demo data"));
    m_csvEnabledCheck->setText(tr("Record valid telemetry to CSV"));
    m_browseButton->setText(tr("Browse..."));
    m_applyButton->setText(tr("Apply and reconnect"));
    m_passwordHint->setText(
        tr("For security, the password is kept in memory only and is not "
           "written to QSettings."));
    m_restartHint->setText(
        tr("Language changes are applied immediately."));
    updateBackendHint();
}
