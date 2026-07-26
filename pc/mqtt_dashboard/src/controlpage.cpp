#include "controlpage.h"

#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

ControlPage::ControlPage(QWidget *parent)
    : QWidget(parent),
      m_timeout(new QTimer(this))
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

    auto *controlPanel = new QFrame(this);
    controlPanel->setObjectName(QStringLiteral("panel"));
    controlPanel->setMinimumHeight(190);
    auto *controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(22, 20, 22, 20);
    controlLayout->setSpacing(16);

    m_connectionHint = new QLabel(controlPanel);
    m_connectionHint->setObjectName(QStringLiteral("mutedLabel"));
    m_connectionHint->setWordWrap(true);
    controlLayout->addWidget(m_connectionHint);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(14);
    m_offButton = new QPushButton(controlPanel);
    m_offButton->setObjectName(QStringLiteral("commandButton"));
    m_onButton = new QPushButton(controlPanel);
    m_onButton->setObjectName(QStringLiteral("commandButton"));
    m_heartbeatButton = new QPushButton(controlPanel);
    m_heartbeatButton->setObjectName(QStringLiteral("commandButton"));
    buttons->addWidget(m_offButton);
    buttons->addWidget(m_onButton);
    buttons->addWidget(m_heartbeatButton);
    controlLayout->addLayout(buttons);

    auto *resultLayout = new QHBoxLayout;
    m_resultTitle = new QLabel(controlPanel);
    m_resultTitle->setObjectName(QStringLiteral("detailTitle"));
    m_resultValue = new QLabel(controlPanel);
    m_resultValue->setObjectName(QStringLiteral("commandResult"));
    m_resultValue->setWordWrap(true);
    resultLayout->addWidget(m_resultTitle);
    resultLayout->addWidget(m_resultValue, 1);
    controlLayout->addLayout(resultLayout);
    root->addWidget(controlPanel);

    m_historyTitle = new QLabel(this);
    m_historyTitle->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(m_historyTitle);
    m_history = new QPlainTextEdit(this);
    m_history->setReadOnly(true);
    m_history->setMaximumBlockCount(500);
    m_history->setObjectName(QStringLiteral("eventLog"));
    root->addWidget(m_history, 1);

    m_timeout->setSingleShot(true);
    m_timeout->setInterval(5000);
    connect(m_timeout, &QTimer::timeout,
            this, &ControlPage::commandTimedOut);
    connect(m_offButton, &QPushButton::clicked,
            this, [this]() { emit ledCommandRequested(0); });
    connect(m_onButton, &QPushButton::clicked,
            this, [this]() { emit ledCommandRequested(1); });
    connect(m_heartbeatButton, &QPushButton::clicked,
            this, [this]() { emit ledCommandRequested(2); });

    retranslateUi();
    setConnected(false);
}

void ControlPage::setConnected(bool connected)
{
    m_connected = connected;
    if (m_pendingValue < 0) {
        setButtonsEnabled(connected);
    }
    m_connectionHint->setText(
        connected
            ? tr("Commands are sent with QoS 1 and require a device response.")
            : tr("Connect to the MQTT Broker before sending commands."));
    if (!connected && m_pendingValue >= 0) {
        m_timeout->stop();
        appendHistory(tr("Connection lost while waiting for a response."));
        m_pendingValue = -1;
        m_resultValue->setText(tr("Disconnected"));
    }
}

void ControlPage::beginCommand(int value)
{
    if (!m_connected || value < 0 || value > 2) {
        return;
    }
    m_pendingValue = value;
    setButtonsEnabled(false);
    m_resultValue->setText(
        tr("Waiting for device response: %1").arg(commandName(value)));
    appendHistory(tr("Published LED command: %1").arg(commandName(value)));
    m_timeout->start();
}

void ControlPage::handleResponse(const CommandResponse &response)
{
    if (response.command != QStringLiteral("led")) {
        return;
    }

    const QString outcome =
        response.success ? tr("Success") : tr("Failed: %1").arg(response.error);
    appendHistory(
        tr("Device response for %1: %2")
            .arg(commandName(response.value), outcome));

    if (m_pendingValue < 0 || response.value != m_pendingValue) {
        return;
    }
    m_timeout->stop();
    m_resultValue->setText(outcome);
    m_pendingValue = -1;
    setButtonsEnabled(m_connected);
}

void ControlPage::handlePublishFailure(const QString &error)
{
    if (m_pendingValue < 0) {
        return;
    }
    m_timeout->stop();
    m_resultValue->setText(tr("Publish failed: %1").arg(error));
    appendHistory(m_resultValue->text());
    m_pendingValue = -1;
    setButtonsEnabled(m_connected);
}

void ControlPage::commandTimedOut()
{
    if (m_pendingValue < 0) {
        return;
    }
    const QString text =
        tr("Device response timeout for %1").arg(commandName(m_pendingValue));
    m_resultValue->setText(text);
    appendHistory(text);
    m_pendingValue = -1;
    setButtonsEnabled(m_connected);
}

void ControlPage::setButtonsEnabled(bool enabled)
{
    m_offButton->setEnabled(enabled);
    m_onButton->setEnabled(enabled);
    m_heartbeatButton->setEnabled(enabled);
}

void ControlPage::appendHistory(const QString &text)
{
    m_history->appendPlainText(
        QStringLiteral("[%1] %2")
            .arg(QDateTime::currentDateTime().toString(
                     QStringLiteral("HH:mm:ss.zzz")),
                 text));
}

QString ControlPage::commandName(int value) const
{
    switch (value) {
    case 0:
        return tr("LED off");
    case 1:
        return tr("LED on");
    case 2:
        return tr("Heartbeat");
    default:
        return tr("Unknown");
    }
}

void ControlPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void ControlPage::retranslateUi()
{
    m_pageTitle->setText(tr("Device Control"));
    m_pageSubtitle->setText(
        tr("Closed-loop LED control through MQTT command responses"));
    m_offButton->setText(tr("0 | LED Off"));
    m_onButton->setText(tr("1 | LED On"));
    m_heartbeatButton->setText(tr("2 | Heartbeat"));
    m_resultTitle->setText(tr("Last result"));
    if (m_pendingValue < 0 && m_resultValue->text().isEmpty()) {
        m_resultValue->setText(tr("No command sent"));
    }
    m_historyTitle->setText(tr("Command history"));
    setConnected(m_connected);
}
