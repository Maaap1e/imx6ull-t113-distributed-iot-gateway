#include "overviewpage.h"

#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

OverviewPage::OverviewPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(18);

    m_pageTitle = new QLabel(this);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    m_pageSubtitle = new QLabel(this);
    m_pageSubtitle->setObjectName(QStringLiteral("pageSubtitle"));
    root->addWidget(m_pageTitle);
    root->addWidget(m_pageSubtitle);

    auto *statusLayout = new QHBoxLayout;
    statusLayout->setSpacing(12);
    statusLayout->addWidget(createStatusCard(
        &m_mqttTitle, &m_mqttState, &m_mqttDetail));
    statusLayout->addWidget(createStatusCard(
        &m_gatewayTitle, &m_gatewayState, &m_gatewayDetail));
    statusLayout->addWidget(createStatusCard(
        &m_tcpTitle, &m_tcpState, &m_tcpDetail));
    statusLayout->addWidget(createStatusCard(
        &m_stm32Title, &m_stm32State, &m_stm32Detail));
    root->addLayout(statusLayout);

    auto *valueLayout = new QHBoxLayout;
    valueLayout->setSpacing(12);
    valueLayout->addWidget(createValueCard(
        &m_lightTitle, &m_lightValue, &m_lightSubtitle));
    valueLayout->addWidget(createValueCard(
        &m_temperatureTitle, &m_temperatureValue,
        &m_temperatureSubtitle));
    valueLayout->addWidget(createValueCard(
        &m_humidityTitle, &m_humidityValue, &m_humiditySubtitle));
    valueLayout->addWidget(createValueCard(
        &m_proximityTitle, &m_proximityValue, &m_proximitySubtitle));
    root->addLayout(valueLayout);

    auto *details = new QGroupBox(this);
    details->setObjectName(QStringLiteral("detailsPanel"));
    details->setMinimumHeight(190);
    auto *detailsLayout = new QGridLayout(details);
    detailsLayout->setContentsMargins(20, 20, 20, 20);
    detailsLayout->setHorizontalSpacing(26);
    detailsLayout->setVerticalSpacing(12);

    m_imuTitle = new QLabel(details);
    m_imuTitle->setObjectName(QStringLiteral("detailTitle"));
    m_imuValues = new QLabel(QStringLiteral("--"), details);
    m_imuValues->setObjectName(QStringLiteral("detailValue"));
    m_imuValues->setWordWrap(true);

    m_runtimeTitle = new QLabel(details);
    m_runtimeTitle->setObjectName(QStringLiteral("detailTitle"));
    m_runtimeValues = new QLabel(QStringLiteral("--"), details);
    m_runtimeValues->setObjectName(QStringLiteral("detailValue"));
    m_runtimeValues->setWordWrap(true);

    m_lastUpdateTitle = new QLabel(details);
    m_lastUpdateTitle->setObjectName(QStringLiteral("detailTitle"));
    m_lastUpdateValue = new QLabel(QStringLiteral("--"), details);
    m_lastUpdateValue->setObjectName(QStringLiteral("detailValue"));

    detailsLayout->addWidget(m_imuTitle, 0, 0);
    detailsLayout->addWidget(m_runtimeTitle, 0, 1);
    detailsLayout->addWidget(m_imuValues, 1, 0);
    detailsLayout->addWidget(m_runtimeValues, 1, 1);
    detailsLayout->addWidget(m_lastUpdateTitle, 2, 0);
    detailsLayout->addWidget(m_lastUpdateValue, 2, 1);
    detailsLayout->setColumnStretch(0, 1);
    detailsLayout->setColumnStretch(1, 1);
    // Let the empty row absorb additional window height so label rows keep
    // their natural size instead of being spread vertically.
    detailsLayout->setRowStretch(3, 1);
    root->addWidget(details, 1);

    setMqttState(false, tr("Not connected"));
    setGatewayOnline(false);
    setBrokerDeviceOnline(false, QString());
    setState(m_tcpState, false, tr("Offline"));
    setState(m_stm32State, false, tr("Offline"));
    retranslateUi();
}

QWidget *OverviewPage::createStatusCard(QLabel **title,
                                        QLabel **state,
                                        QLabel **detail)
{
    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("statusCard"));
    frame->setMinimumHeight(132);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(7);

    *title = new QLabel(frame);
    (*title)->setObjectName(QStringLiteral("cardTitle"));
    *state = new QLabel(frame);
    (*state)->setObjectName(QStringLiteral("stateBadge"));
    *detail = new QLabel(frame);
    (*detail)->setObjectName(QStringLiteral("cardDetail"));
    (*detail)->setWordWrap(true);
    (*detail)->setMinimumHeight(36);
    (*detail)->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    layout->addWidget(*title);
    layout->addWidget(*state);
    layout->addWidget(*detail);
    layout->addStretch();
    return frame;
}

QWidget *OverviewPage::createValueCard(QLabel **title,
                                       QLabel **value,
                                       QLabel **subtitle)
{
    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("valueCard"));
    frame->setMinimumHeight(122);
    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(7);

    *title = new QLabel(frame);
    (*title)->setObjectName(QStringLiteral("cardTitle"));
    *value = new QLabel(QStringLiteral("--"), frame);
    (*value)->setObjectName(QStringLiteral("sensorValue"));
    *subtitle = new QLabel(frame);
    (*subtitle)->setObjectName(QStringLiteral("cardDetail"));

    layout->addWidget(*title);
    layout->addWidget(*value);
    layout->addWidget(*subtitle);
    return frame;
}

void OverviewPage::setState(QLabel *label,
                            bool online,
                            const QString &text)
{
    label->setText(text);
    label->setProperty("online", online);
    label->style()->unpolish(label);
    label->style()->polish(label);
}

void OverviewPage::setMqttState(bool connected, const QString &detail)
{
    setState(m_mqttState, connected,
             connected ? tr("Connected") : tr("Disconnected"));
    m_mqttDetail->setText(detail);
}

void OverviewPage::setGatewayOnline(bool online)
{
    setState(m_gatewayState, online,
             online ? tr("Online") : tr("Offline"));
    m_gatewayDetail->setText(
        online ? tr("Telemetry is fresh")
               : tr("No recent telemetry"));
}

void OverviewPage::setBrokerDeviceOnline(bool online, const QString &reason)
{
    Q_UNUSED(reason)
    // The retained status reflects the i.MX6ULL MQTT bridge. The card detail
    // is updated without treating MQTT transport state as device state.
    if (!online && !reason.isEmpty()) {
        m_gatewayDetail->setText(reason);
    }
}

void OverviewPage::updateTelemetry(const TelemetrySample &sample)
{
    setGatewayOnline(true);
    setState(m_tcpState, sample.tcpConnected,
             sample.tcpConnected ? tr("Online") : tr("Offline"));
    m_tcpDetail->setText(
        tr("Reconnects %1 / Disconnects %2")
            .arg(sample.tcpReconnectCount)
            .arg(sample.tcpDisconnectCount));

    const bool stm32Ready = sample.stm32CanOk && sample.stm32Online;
    setState(m_stm32State, stm32Ready,
             stm32Ready ? tr("Online") : tr("Offline"));
    m_stm32Detail->setText(
        tr("Firmware %1 / Frames %2")
            .arg(sample.appVersion.isEmpty() ? QStringLiteral("--")
                                             : sample.appVersion)
            .arg(sample.frames));

    m_lightValue->setText(QString::number(sample.als));
    m_temperatureValue->setText(
        sample.dhtOk ? QStringLiteral("%1 °C").arg(sample.temperature)
                     : QStringLiteral("--"));
    m_humidityValue->setText(
        sample.dhtOk ? QStringLiteral("%1 %").arg(sample.humidity)
                     : QStringLiteral("--"));
    m_proximityValue->setText(QString::number(sample.ps));

    m_imuValues->setText(
        tr("Gyroscope: X %1  Y %2  Z %3\n"
           "Acceleration: X %4  Y %5  Z %6\n"
           "IMU temperature: %7 °C")
            .arg(sample.gyroX, 0, 'f', 2)
            .arg(sample.gyroY, 0, 'f', 2)
            .arg(sample.gyroZ, 0, 'f', 2)
            .arg(sample.accelX, 0, 'f', 2)
            .arg(sample.accelY, 0, 'f', 2)
            .arg(sample.accelZ, 0, 'f', 2)
            .arg(sample.imuTemperature, 0, 'f', 2));

    m_runtimeValues->setText(
        tr("AP3216C: %1 / ICM20608: %2 / DHT11: %3\n"
           "STM32 counter: %4 / Checksum errors: %5\n"
           "Device timestamp: %6")
            .arg(sample.ap3216cOk ? tr("OK") : tr("Error"))
            .arg(sample.icm20608Ok ? tr("OK") : tr("Error"))
            .arg(sample.dhtOk ? tr("OK") : tr("Error"))
            .arg(sample.counter)
            .arg(sample.checksumErrors)
            .arg(sample.deviceTimestamp));
    m_lastUpdateValue->setText(
        sample.receivedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")));
}

void OverviewPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void OverviewPage::retranslateUi()
{
    m_pageTitle->setText(tr("System Overview"));
    m_pageSubtitle->setText(
        tr("Live health and telemetry from the distributed gateway"));
    m_mqttTitle->setText(tr("MQTT Broker"));
    m_gatewayTitle->setText(tr("i.MX6ULL Gateway"));
    m_tcpTitle->setText(tr("T113 TCP Link"));
    m_stm32Title->setText(tr("STM32 CAN Node"));

    m_lightTitle->setText(tr("Illuminance"));
    m_lightSubtitle->setText(tr("AP3216C ALS"));
    m_temperatureTitle->setText(tr("Temperature"));
    m_temperatureSubtitle->setText(tr("STM32 DHT11"));
    m_humidityTitle->setText(tr("Humidity"));
    m_humiditySubtitle->setText(tr("STM32 DHT11"));
    m_proximityTitle->setText(tr("Proximity"));
    m_proximitySubtitle->setText(tr("AP3216C PS"));

    m_imuTitle->setText(tr("IMU Details"));
    m_runtimeTitle->setText(tr("Runtime Details"));
    m_lastUpdateTitle->setText(tr("PC receive time"));
}
