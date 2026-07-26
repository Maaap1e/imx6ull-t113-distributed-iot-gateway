#ifndef MQTTTRANSPORT_H
#define MQTTTRANSPORT_H

#include "appsettings.h"

#include <QByteArray>
#include <QObject>
#include <QString>

class MqttTransport : public QObject
{
    Q_OBJECT

public:
    explicit MqttTransport(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~MqttTransport() override = default;

    virtual QString backendName() const = 0;
    virtual bool backendAvailable() const = 0;

public slots:
    virtual void connectToBroker(const AppSettings &settings) = 0;
    virtual void disconnectFromBroker() = 0;
    virtual void publishMessage(const QString &topic,
                                const QByteArray &payload,
                                int qos,
                                bool retained) = 0;

signals:
    void connectionChanged(bool connected, const QString &detail);
    void messageReceived(const QString &topic, const QByteArray &payload);
    void publishCompleted(const QString &topic);
    void publishFailed(const QString &topic, const QString &error);
    void logMessage(const QString &message);
};

MqttTransport *createMqttTransport(bool demoMode, QObject *parent = nullptr);
bool pahoBackendCompiled();

#endif
