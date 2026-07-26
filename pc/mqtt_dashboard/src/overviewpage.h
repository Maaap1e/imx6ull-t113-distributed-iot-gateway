#ifndef OVERVIEWPAGE_H
#define OVERVIEWPAGE_H

#include "telemetry.h"

#include <QWidget>

class QLabel;

class OverviewPage : public QWidget
{
    Q_OBJECT

public:
    explicit OverviewPage(QWidget *parent = nullptr);

    void setMqttState(bool connected, const QString &detail);
    void setGatewayOnline(bool online);
    void setBrokerDeviceOnline(bool online, const QString &reason);
    void updateTelemetry(const TelemetrySample &sample);

protected:
    void changeEvent(QEvent *event) override;

private:
    QWidget *createStatusCard(QLabel **title,
                              QLabel **state,
                              QLabel **detail);
    QWidget *createValueCard(QLabel **title,
                             QLabel **value,
                             QLabel **subtitle);
    void setState(QLabel *label, bool online, const QString &text);
    void retranslateUi();

    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageSubtitle = nullptr;

    QLabel *m_mqttTitle = nullptr;
    QLabel *m_mqttState = nullptr;
    QLabel *m_mqttDetail = nullptr;
    QLabel *m_gatewayTitle = nullptr;
    QLabel *m_gatewayState = nullptr;
    QLabel *m_gatewayDetail = nullptr;
    QLabel *m_tcpTitle = nullptr;
    QLabel *m_tcpState = nullptr;
    QLabel *m_tcpDetail = nullptr;
    QLabel *m_stm32Title = nullptr;
    QLabel *m_stm32State = nullptr;
    QLabel *m_stm32Detail = nullptr;

    QLabel *m_lightTitle = nullptr;
    QLabel *m_lightValue = nullptr;
    QLabel *m_lightSubtitle = nullptr;
    QLabel *m_temperatureTitle = nullptr;
    QLabel *m_temperatureValue = nullptr;
    QLabel *m_temperatureSubtitle = nullptr;
    QLabel *m_humidityTitle = nullptr;
    QLabel *m_humidityValue = nullptr;
    QLabel *m_humiditySubtitle = nullptr;
    QLabel *m_proximityTitle = nullptr;
    QLabel *m_proximityValue = nullptr;
    QLabel *m_proximitySubtitle = nullptr;

    QLabel *m_imuTitle = nullptr;
    QLabel *m_imuValues = nullptr;
    QLabel *m_runtimeTitle = nullptr;
    QLabel *m_runtimeValues = nullptr;
    QLabel *m_lastUpdateTitle = nullptr;
    QLabel *m_lastUpdateValue = nullptr;
};

#endif
