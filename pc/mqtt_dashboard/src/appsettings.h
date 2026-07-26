#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QMetaType>
#include <QString>

struct AppSettings
{
    QString brokerHost = QStringLiteral("mq.tongxinmao.com");
    quint16 brokerPort = 18830;
    QString clientId = QStringLiteral("Maaap1e-pc-01");
    QString topicPrefix =
        QStringLiteral("/public/TEST/Maaap1e/imx6ull-01");
    QString username;
    QString password;
    QString csvDirectory;
    QString language = QStringLiteral("zh_CN");
    bool autoConnect = true;
    bool csvEnabled = true;
#ifdef MQTT_DASHBOARD_WITH_PAHO
    bool demoMode = false;
#else
    bool demoMode = true;
#endif

    QString brokerUri() const;
    QString telemetryTopic() const;
    QString statusTopic() const;
    QString commandTopic() const;
    QString responseTopic() const;

    static AppSettings load();
    void save() const;
};

Q_DECLARE_METATYPE(AppSettings)

#endif
