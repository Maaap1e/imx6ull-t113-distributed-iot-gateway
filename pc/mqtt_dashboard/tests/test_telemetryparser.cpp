#include "telemetryparser.h"

#include <QtTest>

class TelemetryParserTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesGatewayTelemetry();
    void rejectsBrokenPayload();
    void parsesCommandResponse();
    void parsesRetainedStatus();
};

void TelemetryParserTest::parsesGatewayTelemetry()
{
    const QByteArray payload = R"JSON({
        "gateway":"imx6ull",
        "timestamp":1784702282,
        "simulated":0,
        "tcp_client":{
            "connected":1,
            "reconnect_count":3,
            "disconnect_count":0,
            "last_connected_time":1784702065,
            "last_error":"none"
        },
        "ap3216c_ok":1,
        "ap3216c":{"als":95,"ir":1,"ps":7},
        "icm20608_ok":1,
        "icm20608":{
            "gyro_x":-1.52,"gyro_y":-0.24,"gyro_z":-0.55,
            "accel_x":0.01,"accel_y":0.01,"accel_z":1.00,
            "temp":34.84
        },
        "stm32_can_ok":1,
        "stm32_can":{
            "node":"stm32f103","online":1,"dht_ok":1,
            "temperature":28,"humidity":23,"app_version":"1.1",
            "counter":206,"frames":414,"checksum_errors":0,
            "last_seen":1784702282
        }
    })JSON";

    TelemetrySample sample;
    QString error;
    QVERIFY2(TelemetryParser::parseTelemetry(payload, &sample, &error),
             qPrintable(error));
    QCOMPARE(sample.deviceTimestamp, qint64(1784702282));
    QVERIFY(sample.tcpConnected);
    QCOMPARE(sample.tcpReconnectCount, quint64(3));
    QCOMPARE(sample.als, 95);
    QCOMPARE(sample.ps, 7);
    QCOMPARE(sample.imuTemperature, 34.84);
    QVERIFY(sample.stm32Online);
    QCOMPARE(sample.temperature, 28);
    QCOMPARE(sample.humidity, 23);
    QCOMPARE(sample.appVersion, QStringLiteral("1.1"));
    QCOMPARE(sample.frames, quint64(414));
    QVERIFY(sample.receivedAt.isValid());
}

void TelemetryParserTest::rejectsBrokenPayload()
{
    TelemetrySample sample;
    QString error;
    QVERIFY(!TelemetryParser::parseTelemetry(
        QByteArrayLiteral("{\"gateway\":\"imx6ull\"}"), &sample, &error));
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(!TelemetryParser::parseTelemetry(
        QByteArrayLiteral("{not-json"), &sample, &error));
    QVERIFY(error.contains(QStringLiteral("invalid JSON")));
}

void TelemetryParserTest::parsesCommandResponse()
{
    const QByteArray payload =
        QByteArrayLiteral("{\"command\":\"led\",\"value\":2,"
                          "\"success\":true,\"error\":\"none\","
                          "\"timestamp\":1784702291}");
    CommandResponse response;
    QString error;
    QVERIFY2(TelemetryParser::parseCommandResponse(
                 payload, &response, &error),
             qPrintable(error));
    QCOMPARE(response.command, QStringLiteral("led"));
    QCOMPARE(response.value, 2);
    QVERIFY(response.success);
    QCOMPARE(response.error, QStringLiteral("none"));
}

void TelemetryParserTest::parsesRetainedStatus()
{
    bool online = false;
    QString reason;
    QString error;
    QVERIFY(TelemetryParser::parseOnlineStatus(
        QByteArrayLiteral("{\"status\":\"online\"}"),
        &online, &reason, &error));
    QVERIFY(online);

    QVERIFY(TelemetryParser::parseOnlineStatus(
        QByteArrayLiteral("{\"status\":\"offline\","
                          "\"reason\":\"unexpected_disconnect\"}"),
        &online, &reason, &error));
    QVERIFY(!online);
    QCOMPARE(reason, QStringLiteral("unexpected_disconnect"));
}

QTEST_APPLESS_MAIN(TelemetryParserTest)

#include "test_telemetryparser.moc"
