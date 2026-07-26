#include "telemetryparser.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {

bool parseObject(const QByteArray &payload, QJsonObject *object, QString *error)
{
    if (!object) {
        if (error) {
            *error = QStringLiteral("internal error: null output object");
        }
        return false;
    }
    if (payload.isEmpty()) {
        if (error) {
            *error = QStringLiteral("empty JSON payload");
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        if (error) {
            *error = QStringLiteral("invalid JSON at offset %1: %2")
                         .arg(parseError.offset)
                         .arg(parseError.errorString());
        }
        return false;
    }
    if (!document.isObject()) {
        if (error) {
            *error = QStringLiteral("JSON root must be an object");
        }
        return false;
    }
    *object = document.object();
    return true;
}

bool requireObject(const QJsonObject &parent,
                   const QString &key,
                   QJsonObject *value,
                   QString *error)
{
    const QJsonValue field = parent.value(key);
    if (!field.isObject()) {
        if (error) {
            *error = QStringLiteral("missing or invalid object: %1").arg(key);
        }
        return false;
    }
    *value = field.toObject();
    return true;
}

bool requireBoolLike(const QJsonObject &object,
                     const QString &key,
                     bool *value,
                     QString *error)
{
    const QJsonValue field = object.value(key);
    if (field.isBool()) {
        *value = field.toBool();
        return true;
    }
    if (field.isDouble()) {
        *value = field.toInt() != 0;
        return true;
    }
    if (error) {
        *error = QStringLiteral("missing or invalid boolean: %1").arg(key);
    }
    return false;
}

bool requireNumber(const QJsonObject &object,
                   const QString &key,
                   double *value,
                   QString *error)
{
    const QJsonValue field = object.value(key);
    if (!field.isDouble()) {
        if (error) {
            *error = QStringLiteral("missing or invalid number: %1").arg(key);
        }
        return false;
    }
    *value = field.toDouble();
    return true;
}

bool requireString(const QJsonObject &object,
                   const QString &key,
                   QString *value,
                   QString *error)
{
    const QJsonValue field = object.value(key);
    if (!field.isString()) {
        if (error) {
            *error = QStringLiteral("missing or invalid string: %1").arg(key);
        }
        return false;
    }
    *value = field.toString();
    return true;
}

quint64 unsignedValue(const QJsonObject &object, const QString &key)
{
    const double value = object.value(key).toDouble(0.0);
    return value > 0.0 ? static_cast<quint64>(value) : 0;
}

qint64 signedValue(const QJsonObject &object, const QString &key)
{
    return static_cast<qint64>(object.value(key).toDouble(0.0));
}

} // namespace

bool TelemetryParser::parseTelemetry(const QByteArray &payload,
                                     TelemetrySample *sample,
                                     QString *error)
{
    if (!sample) {
        if (error) {
            *error = QStringLiteral("internal error: null telemetry output");
        }
        return false;
    }

    QJsonObject root;
    if (!parseObject(payload, &root, error)) {
        return false;
    }
    if (root.value(QStringLiteral("gateway")).toString() !=
        QStringLiteral("imx6ull")) {
        if (error) {
            *error = QStringLiteral("unsupported or missing gateway field");
        }
        return false;
    }

    QJsonObject tcp;
    QJsonObject ap;
    QJsonObject imu;
    QJsonObject stm32;
    if (!requireObject(root, QStringLiteral("tcp_client"), &tcp, error) ||
        !requireObject(root, QStringLiteral("ap3216c"), &ap, error) ||
        !requireObject(root, QStringLiteral("icm20608"), &imu, error) ||
        !requireObject(root, QStringLiteral("stm32_can"), &stm32, error)) {
        return false;
    }

    TelemetrySample parsed;
    parsed.receivedAt = QDateTime::currentDateTime();
    parsed.deviceTimestamp = signedValue(root, QStringLiteral("timestamp"));
    parsed.simulated = root.value(QStringLiteral("simulated")).toInt() != 0;

    if (!requireBoolLike(tcp, QStringLiteral("connected"),
                         &parsed.tcpConnected, error) ||
        !requireBoolLike(root, QStringLiteral("ap3216c_ok"),
                         &parsed.ap3216cOk, error) ||
        !requireBoolLike(root, QStringLiteral("icm20608_ok"),
                         &parsed.icm20608Ok, error) ||
        !requireBoolLike(root, QStringLiteral("stm32_can_ok"),
                         &parsed.stm32CanOk, error)) {
        return false;
    }

    parsed.tcpReconnectCount =
        unsignedValue(tcp, QStringLiteral("reconnect_count"));
    parsed.tcpDisconnectCount =
        unsignedValue(tcp, QStringLiteral("disconnect_count"));
    parsed.tcpLastConnectedTime =
        signedValue(tcp, QStringLiteral("last_connected_time"));
    parsed.tcpLastError =
        tcp.value(QStringLiteral("last_error")).toString(QStringLiteral("none"));

    double number = 0.0;
    if (!requireNumber(ap, QStringLiteral("als"), &number, error)) {
        return false;
    }
    parsed.als = static_cast<int>(number);
    if (!requireNumber(ap, QStringLiteral("ir"), &number, error)) {
        return false;
    }
    parsed.ir = static_cast<int>(number);
    if (!requireNumber(ap, QStringLiteral("ps"), &number, error)) {
        return false;
    }
    parsed.ps = static_cast<int>(number);

    if (!requireNumber(imu, QStringLiteral("gyro_x"), &parsed.gyroX, error) ||
        !requireNumber(imu, QStringLiteral("gyro_y"), &parsed.gyroY, error) ||
        !requireNumber(imu, QStringLiteral("gyro_z"), &parsed.gyroZ, error) ||
        !requireNumber(imu, QStringLiteral("accel_x"), &parsed.accelX, error) ||
        !requireNumber(imu, QStringLiteral("accel_y"), &parsed.accelY, error) ||
        !requireNumber(imu, QStringLiteral("accel_z"), &parsed.accelZ, error) ||
        !requireNumber(imu, QStringLiteral("temp"),
                       &parsed.imuTemperature, error)) {
        return false;
    }

    parsed.stm32Node = stm32.value(QStringLiteral("node")).toString();
    if (!requireBoolLike(stm32, QStringLiteral("online"),
                         &parsed.stm32Online, error) ||
        !requireBoolLike(stm32, QStringLiteral("dht_ok"),
                         &parsed.dhtOk, error)) {
        return false;
    }
    parsed.temperature =
        static_cast<int>(stm32.value(QStringLiteral("temperature")).toDouble());
    parsed.humidity =
        static_cast<int>(stm32.value(QStringLiteral("humidity")).toDouble());
    parsed.appVersion =
        stm32.value(QStringLiteral("app_version")).toString();
    parsed.counter = unsignedValue(stm32, QStringLiteral("counter"));
    parsed.frames = unsignedValue(stm32, QStringLiteral("frames"));
    parsed.checksumErrors =
        unsignedValue(stm32, QStringLiteral("checksum_errors"));
    parsed.stm32LastSeen =
        signedValue(stm32, QStringLiteral("last_seen"));

    *sample = parsed;
    if (error) {
        error->clear();
    }
    return true;
}

bool TelemetryParser::parseCommandResponse(const QByteArray &payload,
                                           CommandResponse *response,
                                           QString *error)
{
    if (!response) {
        if (error) {
            *error = QStringLiteral("internal error: null command output");
        }
        return false;
    }
    QJsonObject root;
    if (!parseObject(payload, &root, error)) {
        return false;
    }

    CommandResponse parsed;
    if (!requireString(root, QStringLiteral("command"),
                       &parsed.command, error) ||
        !requireBoolLike(root, QStringLiteral("success"),
                         &parsed.success, error)) {
        return false;
    }
    if (!root.value(QStringLiteral("value")).isDouble()) {
        if (error) {
            *error = QStringLiteral("missing or invalid number: value");
        }
        return false;
    }
    parsed.value = root.value(QStringLiteral("value")).toInt(-1);
    parsed.error =
        root.value(QStringLiteral("error")).toString(QStringLiteral("unknown"));
    parsed.deviceTimestamp =
        signedValue(root, QStringLiteral("timestamp"));

    if (parsed.command != QStringLiteral("led") ||
        parsed.value < 0 || parsed.value > 2) {
        if (error) {
            *error = QStringLiteral("unsupported command response");
        }
        return false;
    }
    *response = parsed;
    if (error) {
        error->clear();
    }
    return true;
}

bool TelemetryParser::parseOnlineStatus(const QByteArray &payload,
                                        bool *online,
                                        QString *reason,
                                        QString *error)
{
    if (!online) {
        if (error) {
            *error = QStringLiteral("internal error: null status output");
        }
        return false;
    }
    QJsonObject root;
    if (!parseObject(payload, &root, error)) {
        return false;
    }
    const QString state = root.value(QStringLiteral("status")).toString();
    if (state != QStringLiteral("online") &&
        state != QStringLiteral("offline")) {
        if (error) {
            *error = QStringLiteral("status must be online or offline");
        }
        return false;
    }
    *online = state == QStringLiteral("online");
    if (reason) {
        *reason = root.value(QStringLiteral("reason")).toString();
    }
    if (error) {
        error->clear();
    }
    return true;
}
