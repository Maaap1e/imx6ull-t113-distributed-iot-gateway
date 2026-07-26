#include "mainwindow.h"

#include "controlpage.h"
#include "csvrecorder.h"
#include "mqtttransport.h"
#include "overviewpage.h"
#include "settingspage.h"
#include "telemetryparser.h"
#include "trendspage.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QEvent>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QStatusBar>
#include <QTimer>
#include <QTranslator>
#include <QVariant>
#include <QVBoxLayout>

#include <algorithm>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_settings(AppSettings::load()),
      m_csvRecorder(new CsvRecorder(this)),
      m_translator(new QTranslator(this)),
      m_freshnessTimer(new QTimer(this))
{
    qRegisterMetaType<AppSettings>("AppSettings");
    qRegisterMetaType<TelemetrySample>("TelemetrySample");
    qRegisterMetaType<CommandResponse>("CommandResponse");

    const QStringList arguments = qApp->arguments();
    const bool smokeTest =
        arguments.contains(QStringLiteral("--smoke-test"));
    const bool liveTest =
        arguments.contains(QStringLiteral("--live-test"));
    if (smokeTest) {
        m_settings.demoMode = true;
        m_settings.autoConnect = true;
        m_settings.csvEnabled = false;
        m_settings.language = QStringLiteral("zh_CN");
    } else if (liveTest) {
        m_settings.demoMode = false;
        m_settings.autoConnect = true;
        m_settings.csvEnabled = false;
        m_settings.language = QStringLiteral("zh_CN");
    }

    applyLanguage(m_settings.language);
    buildUi();
    loadStyleSheet();

    m_csvRecorder->configure(m_settings.csvEnabled,
                             m_settings.csvDirectory);
    m_settingsPage->setSettings(m_settings);
    recreateTransport();

    connect(m_connectButton, &QPushButton::clicked,
            this, &MainWindow::toggleConnection);
    connect(m_navigation,
            QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &MainWindow::showPage);
    connect(m_controlPage, &ControlPage::ledCommandRequested,
            this, &MainWindow::handleLedCommand);
    connect(m_settingsPage, &SettingsPage::settingsApplied,
            this, &MainWindow::handleSettingsApplied);
    connect(m_csvRecorder, &CsvRecorder::errorOccurred,
            this, [this](const QString &message) {
                setFooterMessage(message, true);
            });
    connect(m_csvRecorder, &CsvRecorder::fileChanged,
            this, [this](const QString &path) {
                m_csvLabel->setText(tr("CSV: %1").arg(path));
            });

    m_freshnessTimer->setInterval(1000);
    connect(m_freshnessTimer, &QTimer::timeout,
            this, &MainWindow::checkTelemetryFreshness);
    m_freshnessTimer->start();

    // Keep enough vertical room for wrapped bilingual labels at common
    // Windows display-scaling values.  The initial height is 25% larger than
    // the original 800 px layout.
    setMinimumSize(1100, 875);
    resize(1280, 1000);
    showPage(0);
    retranslateUi();
    updateCounters();

    if (m_settings.autoConnect) {
        QTimer::singleShot(0, this, &MainWindow::requestConnection);
    }
}

MainWindow::~MainWindow()
{
    if (m_transport) {
        m_transport->disconnectFromBroker();
    }
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("appRoot"));
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *header = new QFrame(central);
    header->setObjectName(QStringLiteral("appHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(22, 12, 22, 12);
    headerLayout->setSpacing(12);
    m_titleLabel = new QLabel(header);
    m_titleLabel->setObjectName(QStringLiteral("appTitle"));
    m_brokerLabel = new QLabel(header);
    m_brokerLabel->setObjectName(QStringLiteral("brokerText"));
    m_connectionBadge = new QLabel(header);
    m_connectionBadge->setObjectName(QStringLiteral("connectionBadge"));
    m_connectButton = new QPushButton(header);
    m_connectButton->setObjectName(QStringLiteral("headerButton"));

    headerLayout->addWidget(m_titleLabel);
    headerLayout->addSpacing(18);
    headerLayout->addWidget(m_brokerLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_connectionBadge);
    headerLayout->addWidget(m_connectButton);
    outer->addWidget(header);

    auto *body = new QWidget(central);
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    auto *sidebar = new QFrame(body);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(190);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(14, 22, 14, 18);
    sideLayout->setSpacing(8);

    m_overviewButton = new QPushButton(sidebar);
    m_trendsButton = new QPushButton(sidebar);
    m_controlButton = new QPushButton(sidebar);
    m_settingsButton = new QPushButton(sidebar);
    const QList<QPushButton *> buttons = {
        m_overviewButton, m_trendsButton, m_controlButton, m_settingsButton
    };
    m_navigation = new QButtonGroup(this);
    m_navigation->setExclusive(true);
    for (int i = 0; i < buttons.size(); ++i) {
        buttons.at(i)->setCheckable(true);
        buttons.at(i)->setObjectName(QStringLiteral("navigationButton"));
        m_navigation->addButton(buttons.at(i), i);
        sideLayout->addWidget(buttons.at(i));
    }
    sideLayout->addStretch();
    auto *version = new QLabel(QStringLiteral("PC Dashboard 1.1"), sidebar);
    version->setObjectName(QStringLiteral("sidebarVersion"));
    sideLayout->addWidget(version);

    m_pages = new QStackedWidget(body);
    m_pages->setObjectName(QStringLiteral("contentPages"));
    m_overviewPage = new OverviewPage(m_pages);
    m_trendsPage = new TrendsPage(m_pages);
    m_controlPage = new ControlPage(m_pages);
    m_settingsPage = new SettingsPage(m_pages);
    m_pages->addWidget(m_overviewPage);
    m_pages->addWidget(m_trendsPage);
    m_pages->addWidget(m_controlPage);
    m_pages->addWidget(m_settingsPage);

    bodyLayout->addWidget(sidebar);
    bodyLayout->addWidget(m_pages, 1);
    outer->addWidget(body, 1);

    auto *footer = new QFrame(central);
    footer->setObjectName(QStringLiteral("appFooter"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(18, 7, 18, 7);
    m_footerMessage = new QLabel(footer);
    m_footerMessage->setObjectName(QStringLiteral("footerMessage"));
    m_counterLabel = new QLabel(footer);
    m_counterLabel->setObjectName(QStringLiteral("footerInfo"));
    m_csvLabel = new QLabel(footer);
    m_csvLabel->setObjectName(QStringLiteral("footerInfo"));
    footerLayout->addWidget(m_footerMessage, 1);
    footerLayout->addWidget(m_counterLabel);
    footerLayout->addSpacing(14);
    footerLayout->addWidget(m_csvLabel);
    outer->addWidget(footer);

    setCentralWidget(central);
}

void MainWindow::recreateTransport()
{
    if (m_transport) {
        m_transport->disconnectFromBroker();
        delete m_transport;
        m_transport = nullptr;
    }
    m_connected = false;
    m_connectionRequested = false;
    m_transport = createMqttTransport(m_settings.demoMode, this);
    connectTransportSignals();
    setHeaderState(false, tr("Not connected"));
    m_controlPage->setConnected(false);
    m_brokerLabel->setText(
        m_settings.demoMode
            ? tr("Source: built-in demo")
            : tr("Broker: %1").arg(m_settings.brokerUri()));
}

void MainWindow::connectTransportSignals()
{
    connect(m_transport, &MqttTransport::connectionChanged,
            this, &MainWindow::handleConnectionChanged);
    connect(m_transport, &MqttTransport::messageReceived,
            this, &MainWindow::handleMessage);
    connect(m_transport, &MqttTransport::publishFailed,
            this, [this](const QString &topic, const QString &error) {
                if (topic == m_settings.commandTopic()) {
                    m_controlPage->handlePublishFailure(error);
                }
                setFooterMessage(error, true);
            });
    connect(m_transport, &MqttTransport::publishCompleted,
            this, [this](const QString &topic) {
                if (topic == m_settings.commandTopic()) {
                    setFooterMessage(tr("Command published; waiting for "
                                        "the device response."));
                }
            });
    connect(m_transport, &MqttTransport::logMessage,
            this, [this](const QString &message) {
                setFooterMessage(message);
            });
}

void MainWindow::toggleConnection()
{
    if (m_connectionRequested) {
        m_connectionRequested = false;
        m_transport->disconnectFromBroker();
        return;
    }
    requestConnection();
}

void MainWindow::requestConnection()
{
    if (!m_transport) {
        return;
    }
    m_connectionRequested = true;
    m_connectButton->setText(tr("Disconnect"));
    setHeaderState(false, tr("Connecting..."));
    m_transport->connectToBroker(m_settings);
}

void MainWindow::handleConnectionChanged(bool connected,
                                         const QString &detail)
{
    m_connected = connected;
    if (!connected && !m_connectionRequested) {
        m_connectButton->setText(tr("Connect"));
    } else {
        m_connectButton->setText(tr("Disconnect"));
    }
    setHeaderState(connected, detail);
    m_overviewPage->setMqttState(connected, detail);
    m_controlPage->setConnected(connected);
    setFooterMessage(detail, !connected && m_connectionRequested);
}

void MainWindow::handleMessage(const QString &topic,
                               const QByteArray &payload)
{
    ++m_receivedMessages;
    if (topic == m_settings.telemetryTopic()) {
        TelemetrySample sample;
        QString error;
        if (!TelemetryParser::parseTelemetry(payload, &sample, &error)) {
            ++m_parseErrors;
            setFooterMessage(tr("Telemetry rejected: %1").arg(error), true);
            updateCounters();
            return;
        }
        m_lastTelemetry = sample.receivedAt;
        m_overviewPage->updateTelemetry(sample);
        m_trendsPage->appendTelemetry(sample);
        if (!m_csvRecorder->record(sample)) {
            ++m_parseErrors;
        }
    } else if (topic == m_settings.statusTopic()) {
        bool online = false;
        QString reason;
        QString error;
        if (TelemetryParser::parseOnlineStatus(payload, &online,
                                               &reason, &error)) {
            m_overviewPage->setBrokerDeviceOnline(online, reason);
            setFooterMessage(
                online ? tr("Gateway status: online")
                       : tr("Gateway status: offline (%1)").arg(reason));
        } else {
            ++m_parseErrors;
            setFooterMessage(tr("Status rejected: %1").arg(error), true);
        }
    } else if (topic == m_settings.responseTopic()) {
        CommandResponse response;
        QString error;
        if (TelemetryParser::parseCommandResponse(payload, &response,
                                                  &error)) {
            m_controlPage->handleResponse(response);
        } else {
            ++m_parseErrors;
            setFooterMessage(
                tr("Command response rejected: %1").arg(error), true);
        }
    }
    updateCounters();
}

void MainWindow::handleLedCommand(int value)
{
    if (!m_connected || !m_transport) {
        setFooterMessage(tr("MQTT is not connected."), true);
        return;
    }
    m_controlPage->beginCommand(value);
    m_transport->publishMessage(
        m_settings.commandTopic(),
        QByteArray::number(value), 1, false);
}

void MainWindow::handleSettingsApplied(const AppSettings &settings)
{
    const bool languageChanged = settings.language != m_settings.language;
    m_settings = settings;
    m_settings.save();
    m_csvRecorder->configure(m_settings.csvEnabled,
                             m_settings.csvDirectory);
    if (languageChanged) {
        applyLanguage(m_settings.language);
    }
    recreateTransport();
    m_settingsPage->setSettings(m_settings);
    setFooterMessage(tr("Settings saved."));
    if (m_settings.autoConnect) {
        requestConnection();
    }
}

void MainWindow::checkTelemetryFreshness()
{
    if (!m_lastTelemetry.isValid()) {
        m_overviewPage->setGatewayOnline(false);
        return;
    }
    const bool fresh =
        m_lastTelemetry.msecsTo(QDateTime::currentDateTime()) <= 15000;
    m_overviewPage->setGatewayOnline(fresh);
}

void MainWindow::showPage(int index)
{
    if (index < 0 || index >= m_pages->count()) {
        return;
    }
    m_pages->setCurrentIndex(index);
    if (QAbstractButton *button = m_navigation->button(index)) {
        button->setChecked(true);
    }
}

void MainWindow::applyLanguage(const QString &language)
{
    qApp->removeTranslator(m_translator);
    if (language == QStringLiteral("zh_CN")) {
        if (!m_translator->load(
                QStringLiteral(":/i18n/mqtt_dashboard_zh_CN.qm"))) {
            m_translator->load(
                QStringLiteral("mqtt_dashboard_zh_CN"),
                qApp->applicationDirPath() + QStringLiteral("/translations"));
        }
        qApp->installTranslator(m_translator);
    }
}

void MainWindow::loadStyleSheet()
{
    QFile file(QStringLiteral(":/styles/dashboard.qss"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
    }
}

void MainWindow::setHeaderState(bool online, const QString &detail)
{
    m_connectionBadge->setText(
        online ? tr("● Connected") : tr("● Offline"));
    m_connectionBadge->setToolTip(detail);
    m_connectionBadge->setProperty("online", online);
    m_connectionBadge->style()->unpolish(m_connectionBadge);
    m_connectionBadge->style()->polish(m_connectionBadge);
}

void MainWindow::setFooterMessage(const QString &message, bool error)
{
    m_footerMessage->setText(message);
    m_footerMessage->setProperty("error", error);
    m_footerMessage->style()->unpolish(m_footerMessage);
    m_footerMessage->style()->polish(m_footerMessage);
}

void MainWindow::updateCounters()
{
    m_counterLabel->setText(
        tr("Messages %1 / Parse errors %2")
            .arg(m_receivedMessages)
            .arg(m_parseErrors));
    if (!m_csvRecorder->isEnabled()) {
        m_csvLabel->setText(tr("CSV: disabled"));
    } else if (m_csvRecorder->currentFilePath().isEmpty()) {
        m_csvLabel->setText(tr("CSV: waiting for data"));
    }
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_transport) {
        m_connectionRequested = false;
        m_transport->disconnectFromBroker();
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("Distributed IoT Gateway - MQTT Dashboard"));
    m_titleLabel->setText(tr("Distributed IoT Gateway"));
    m_overviewButton->setText(tr("Overview"));
    m_trendsButton->setText(tr("Trends"));
    m_controlButton->setText(tr("Control"));
    m_settingsButton->setText(tr("Settings"));
    m_connectButton->setText(
        m_connectionRequested ? tr("Disconnect") : tr("Connect"));
    m_brokerLabel->setText(
        m_settings.demoMode
            ? tr("Source: built-in demo")
            : tr("Broker: %1").arg(m_settings.brokerUri()));
    setHeaderState(m_connected,
                   m_connectionBadge->toolTip());
    updateCounters();
}
