#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QDateTime>
#include <QMetaType>
#include <QString>

struct TelemetrySample
{
    QDateTime receivedAt;
    qint64 deviceTimestamp = 0;
    bool simulated = false;

    bool tcpConnected = false;
    quint64 tcpReconnectCount = 0;
    quint64 tcpDisconnectCount = 0;
    qint64 tcpLastConnectedTime = 0;
    QString tcpLastError;

    bool ap3216cOk = false;
    int als = 0;
    int ir = 0;
    int ps = 0;

    bool icm20608Ok = false;
    double gyroX = 0.0;
    double gyroY = 0.0;
    double gyroZ = 0.0;
    double accelX = 0.0;
    double accelY = 0.0;
    double accelZ = 0.0;
    double imuTemperature = 0.0;

    bool stm32CanOk = false;
    QString stm32Node;
    bool stm32Online = false;
    bool dhtOk = false;
    int temperature = 0;
    int humidity = 0;
    QString appVersion;
    quint64 counter = 0;
    quint64 frames = 0;
    quint64 checksumErrors = 0;
    qint64 stm32LastSeen = 0;
};

struct CommandResponse
{
    QString command;
    int value = -1;
    bool success = false;
    QString error;
    qint64 deviceTimestamp = 0;
};

Q_DECLARE_METATYPE(TelemetrySample)
Q_DECLARE_METATYPE(CommandResponse)

#endif
