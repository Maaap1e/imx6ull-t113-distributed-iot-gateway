#include "appsettings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

QString normalizedPrefix(QString prefix)
{
    prefix = prefix.trimmed();
    while (prefix.endsWith(QLatin1Char('/'))) {
        prefix.chop(1);
    }
    if (!prefix.startsWith(QLatin1Char('/'))) {
        prefix.prepend(QLatin1Char('/'));
    }
    return prefix;
}

} // namespace

QString AppSettings::brokerUri() const
{
    return QStringLiteral("tcp://%1:%2").arg(brokerHost.trimmed())
        .arg(brokerPort);
}

QString AppSettings::telemetryTopic() const
{
    return normalizedPrefix(topicPrefix) + QStringLiteral("/telemetry");
}

QString AppSettings::statusTopic() const
{
    return normalizedPrefix(topicPrefix) + QStringLiteral("/status");
}

QString AppSettings::commandTopic() const
{
    return normalizedPrefix(topicPrefix) + QStringLiteral("/cmd/led");
}

QString AppSettings::responseTopic() const
{
    return normalizedPrefix(topicPrefix) + QStringLiteral("/cmd/response");
}

AppSettings AppSettings::load()
{
    QSettings store(QStringLiteral("Maaap1e"),
                    QStringLiteral("IoTGatewayMqttDashboard"));
    AppSettings result;

    result.brokerHost =
        store.value(QStringLiteral("mqtt/host"), result.brokerHost).toString();
    result.brokerPort = static_cast<quint16>(
        store.value(QStringLiteral("mqtt/port"), result.brokerPort).toUInt());
    result.clientId =
        store.value(QStringLiteral("mqtt/clientId"), result.clientId).toString();
    result.topicPrefix = store
                             .value(QStringLiteral("mqtt/topicPrefix"),
                                    result.topicPrefix)
                             .toString();
    result.username =
        store.value(QStringLiteral("mqtt/username"), result.username).toString();
    result.autoConnect =
        store.value(QStringLiteral("mqtt/autoConnect"), result.autoConnect)
            .toBool();
    result.demoMode =
        store.value(QStringLiteral("mqtt/demoMode"), result.demoMode).toBool();
    result.csvEnabled =
        store.value(QStringLiteral("csv/enabled"), result.csvEnabled).toBool();
    result.csvDirectory =
        store.value(QStringLiteral("csv/directory")).toString();
    result.language =
        store.value(QStringLiteral("ui/language"), result.language).toString();

    if (result.csvDirectory.isEmpty()) {
        const QString documents =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        result.csvDirectory =
            QDir(documents).filePath(QStringLiteral("IoTGatewayCsv"));
    }

    // Passwords are intentionally not loaded from plaintext QSettings.
    result.password.clear();
    result.topicPrefix = normalizedPrefix(result.topicPrefix);
    return result;
}

void AppSettings::save() const
{
    QSettings store(QStringLiteral("Maaap1e"),
                    QStringLiteral("IoTGatewayMqttDashboard"));
    store.setValue(QStringLiteral("mqtt/host"), brokerHost.trimmed());
    store.setValue(QStringLiteral("mqtt/port"), brokerPort);
    store.setValue(QStringLiteral("mqtt/clientId"), clientId.trimmed());
    store.setValue(QStringLiteral("mqtt/topicPrefix"),
                   normalizedPrefix(topicPrefix));
    store.setValue(QStringLiteral("mqtt/username"), username.trimmed());
    store.setValue(QStringLiteral("mqtt/autoConnect"), autoConnect);
    store.setValue(QStringLiteral("mqtt/demoMode"), demoMode);
    store.setValue(QStringLiteral("csv/enabled"), csvEnabled);
    store.setValue(QStringLiteral("csv/directory"), csvDirectory);
    store.setValue(QStringLiteral("ui/language"), language);
    store.sync();
}
