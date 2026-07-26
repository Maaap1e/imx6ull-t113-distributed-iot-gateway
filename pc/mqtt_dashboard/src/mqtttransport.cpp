#include "mqtttransport.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <cstring>

#ifdef MQTT_DASHBOARD_WITH_PAHO
#include <MQTTClient.h>
#endif

namespace {

class DemoMqttTransport final : public MqttTransport
{
    Q_OBJECT

public:
    explicit DemoMqttTransport(QObject *parent = nullptr)
        : MqttTransport(parent)
    {
        m_timer.setInterval(5000);
        connect(&m_timer, &QTimer::timeout,
                this, &DemoMqttTransport::publishDemoTelemetry);
    }

    QString backendName() const override
    {
        return tr("Built-in demo");
    }

    bool backendAvailable() const override
    {
        return true;
    }

public slots:
    void connectToBroker(const AppSettings &settings) override
    {
        m_settings = settings;
        if (m_connected) {
            return;
        }
        m_connected = true;
        emit connectionChanged(true, tr("Demo data source connected"));
        emit logMessage(tr("Demo mode enabled; no Broker connection is used."));

        QJsonObject status;
        status.insert(QStringLiteral("status"), QStringLiteral("online"));
        emit messageReceived(m_settings.statusTopic(),
                             QJsonDocument(status).toJson(QJsonDocument::Compact));
        publishDemoTelemetry();
        m_timer.start();
    }

    void disconnectFromBroker() override
    {
        m_timer.stop();
        if (!m_connected) {
            return;
        }
        m_connected = false;
        emit connectionChanged(false, tr("Disconnected"));
    }

    void publishMessage(const QString &topic,
                        const QByteArray &payload,
                        int qos,
                        bool retained) override
    {
        Q_UNUSED(qos)
        Q_UNUSED(retained)
        if (!m_connected) {
            emit publishFailed(topic, tr("Demo source is disconnected."));
            return;
        }
        emit publishCompleted(topic);
        if (topic != m_settings.commandTopic()) {
            return;
        }

        bool ok = false;
        const int value = QString::fromLatin1(payload).trimmed().toInt(&ok);
        QTimer::singleShot(250, this, [this, value, ok]() {
            QJsonObject response;
            response.insert(QStringLiteral("command"), QStringLiteral("led"));
            response.insert(QStringLiteral("value"), value);
            response.insert(QStringLiteral("success"),
                            ok && value >= 0 && value <= 2);
            response.insert(QStringLiteral("error"),
                            ok && value >= 0 && value <= 2
                                ? QStringLiteral("none")
                                : QStringLiteral("invalid_payload"));
            response.insert(QStringLiteral("timestamp"),
                            QDateTime::currentSecsSinceEpoch());
            emit messageReceived(
                m_settings.responseTopic(),
                QJsonDocument(response).toJson(QJsonDocument::Compact));
        });
    }

private slots:
    void publishDemoTelemetry()
    {
        if (!m_connected) {
            return;
        }
        ++m_counter;
        const double wave = static_cast<double>(m_counter % 20) / 20.0;

        QJsonObject tcp;
        tcp.insert(QStringLiteral("connected"), 1);
        tcp.insert(QStringLiteral("reconnect_count"), 0);
        tcp.insert(QStringLiteral("disconnect_count"), 0);
        tcp.insert(QStringLiteral("last_connected_time"),
                   QDateTime::currentSecsSinceEpoch() - 60);
        tcp.insert(QStringLiteral("last_error"), QStringLiteral("none"));

        QJsonObject ap;
        ap.insert(QStringLiteral("als"), 80 + static_cast<int>(wave * 40));
        ap.insert(QStringLiteral("ir"), static_cast<int>(m_counter % 7));
        ap.insert(QStringLiteral("ps"), 20 + static_cast<int>(wave * 15));

        QJsonObject imu;
        imu.insert(QStringLiteral("gyro_x"), -1.40 + wave * 0.20);
        imu.insert(QStringLiteral("gyro_y"), -0.30);
        imu.insert(QStringLiteral("gyro_z"), -0.49);
        imu.insert(QStringLiteral("accel_x"), 0.01);
        imu.insert(QStringLiteral("accel_y"), -0.01);
        imu.insert(QStringLiteral("accel_z"), 0.99);
        imu.insert(QStringLiteral("temp"), 34.2 + wave);

        QJsonObject stm32;
        stm32.insert(QStringLiteral("node"), QStringLiteral("stm32f103"));
        stm32.insert(QStringLiteral("online"), 1);
        stm32.insert(QStringLiteral("dht_ok"), 1);
        stm32.insert(QStringLiteral("temperature"),
                     27 + static_cast<int>(m_counter % 3));
        stm32.insert(QStringLiteral("humidity"),
                     35 + static_cast<int>(m_counter % 5));
        stm32.insert(QStringLiteral("app_version"), QStringLiteral("1.1"));
        stm32.insert(QStringLiteral("counter"),
                     static_cast<int>(m_counter % 256));
        stm32.insert(QStringLiteral("frames"),
                     static_cast<double>(m_counter * 2));
        stm32.insert(QStringLiteral("checksum_errors"), 0);
        stm32.insert(QStringLiteral("last_seen"),
                     QDateTime::currentSecsSinceEpoch());

        QJsonObject root;
        root.insert(QStringLiteral("gateway"), QStringLiteral("imx6ull"));
        root.insert(QStringLiteral("timestamp"),
                    QDateTime::currentSecsSinceEpoch());
        root.insert(QStringLiteral("simulated"), 1);
        root.insert(QStringLiteral("tcp_client"), tcp);
        root.insert(QStringLiteral("ap3216c_ok"), 1);
        root.insert(QStringLiteral("ap3216c"), ap);
        root.insert(QStringLiteral("icm20608_ok"), 1);
        root.insert(QStringLiteral("icm20608"), imu);
        root.insert(QStringLiteral("stm32_can_ok"), 1);
        root.insert(QStringLiteral("stm32_can"), stm32);

        emit messageReceived(m_settings.telemetryTopic(),
                             QJsonDocument(root).toJson(QJsonDocument::Compact));
    }

private:
    AppSettings m_settings;
    QTimer m_timer;
    bool m_connected = false;
    quint64 m_counter = 0;
};

class UnavailableMqttTransport final : public MqttTransport
{
    Q_OBJECT

public:
    explicit UnavailableMqttTransport(QObject *parent = nullptr)
        : MqttTransport(parent)
    {
    }

    QString backendName() const override
    {
        return tr("Paho MQTT C");
    }

    bool backendAvailable() const override
    {
        return false;
    }

public slots:
    void connectToBroker(const AppSettings &settings) override
    {
        Q_UNUSED(settings)
        const QString detail =
            tr("Paho backend was not included in this build. "
               "Enable Demo mode or rebuild with PAHO_ROOT.");
        emit logMessage(detail);
        emit connectionChanged(false, detail);
    }

    void disconnectFromBroker() override
    {
        emit connectionChanged(false, tr("Disconnected"));
    }

    void publishMessage(const QString &topic,
                        const QByteArray &payload,
                        int qos,
                        bool retained) override
    {
        Q_UNUSED(payload)
        Q_UNUSED(qos)
        Q_UNUSED(retained)
        emit publishFailed(topic, tr("Paho backend is unavailable."));
    }
};

#ifdef MQTT_DASHBOARD_WITH_PAHO

class PahoWorker final : public QObject
{
    Q_OBJECT

public:
    explicit PahoWorker(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

public slots:
    void connectBroker(const AppSettings &settings)
    {
        shutdown();
        m_settings = settings;

        const QByteArray uri = settings.brokerUri().toUtf8();
        const QByteArray clientId = settings.clientId.toUtf8();
        int rc = MQTTClient_create(&m_client, uri.constData(),
                                   clientId.constData(),
                                   MQTTCLIENT_PERSISTENCE_NONE, nullptr);
        if (rc != MQTTCLIENT_SUCCESS) {
            m_client = nullptr;
            emit connectionResult(false,
                                  tr("MQTTClient_create failed: %1").arg(rc));
            return;
        }

        rc = MQTTClient_setCallbacks(m_client, this,
                                     &PahoWorker::connectionLostCallback,
                                     &PahoWorker::messageArrivedCallback,
                                     nullptr);
        if (rc != MQTTCLIENT_SUCCESS) {
            shutdown();
            emit connectionResult(false,
                                  tr("MQTTClient_setCallbacks failed: %1")
                                      .arg(rc));
            return;
        }

        MQTTClient_connectOptions options =
            MQTTClient_connectOptions_initializer;
        options.keepAliveInterval = 30;
        options.cleansession = 1;
        options.connectTimeout = 5;
        options.MQTTVersion = MQTTVERSION_3_1_1;

        const QByteArray username = settings.username.toUtf8();
        const QByteArray password = settings.password.toUtf8();
        if (!username.isEmpty()) {
            options.username = username.constData();
        }
        if (!password.isEmpty()) {
            options.password = password.constData();
        }

        rc = MQTTClient_connect(m_client, &options);
        if (rc != MQTTCLIENT_SUCCESS) {
            shutdown();
            emit connectionResult(false,
                                  tr("MQTT connect failed: %1").arg(rc));
            return;
        }

        const QStringList topics = {
            settings.telemetryTopic(),
            settings.statusTopic(),
            settings.responseTopic()
        };
        for (const QString &topic : topics) {
            const QByteArray encoded = topic.toUtf8();
            rc = MQTTClient_subscribe(m_client, encoded.constData(), 1);
            if (rc != MQTTCLIENT_SUCCESS) {
                shutdown();
                emit connectionResult(
                    false,
                    tr("MQTT subscribe failed for %1: %2").arg(topic).arg(rc));
                return;
            }
        }

        m_connected = true;
        emit connectionResult(true, tr("Connected to %1")
                                        .arg(settings.brokerUri()));
    }

    void disconnectBroker()
    {
        shutdown();
        emit connectionResult(false, tr("Disconnected"));
    }

    void publish(const QString &topic,
                 const QByteArray &payload,
                 int qos,
                 bool retained)
    {
        if (!m_client || !m_connected) {
            emit publishError(topic, tr("MQTT is not connected."));
            return;
        }

        MQTTClient_message message = MQTTClient_message_initializer;
        message.payload = const_cast<char *>(payload.constData());
        message.payloadlen = payload.size();
        message.qos = qos;
        message.retained = retained ? 1 : 0;
        MQTTClient_deliveryToken token = 0;
        const QByteArray encodedTopic = topic.toUtf8();

        int rc = MQTTClient_publishMessage(
            m_client, encodedTopic.constData(), &message,
            qos > 0 ? &token : nullptr);
        if (rc == MQTTCLIENT_SUCCESS && qos > 0) {
            rc = MQTTClient_waitForCompletion(m_client, token, 5000L);
        }
        if (rc == MQTTCLIENT_SUCCESS) {
            emit publishOk(topic);
        } else {
            emit publishError(topic,
                              tr("MQTT publish failed: %1").arg(rc));
        }
    }

    void shutdown()
    {
        if (m_client) {
            if (m_connected) {
                MQTTClient_disconnect(m_client, 1000);
            }
            MQTTClient_destroy(&m_client);
        }
        m_client = nullptr;
        m_connected = false;
    }

signals:
    void connectionResult(bool connected, const QString &detail);
    void message(const QString &topic, const QByteArray &payload);
    void publishOk(const QString &topic);
    void publishError(const QString &topic, const QString &error);

private:
    static void connectionLostCallback(void *context, char *cause)
    {
        PahoWorker *worker = static_cast<PahoWorker *>(context);
        worker->m_connected = false;
        emit worker->connectionResult(
            false,
            cause ? tr("MQTT connection lost: %1")
                        .arg(QString::fromLocal8Bit(cause))
                  : tr("MQTT connection lost"));
    }

    static int messageArrivedCallback(void *context,
                                      char *topicName,
                                      int topicLength,
                                      MQTTClient_message *message)
    {
        PahoWorker *worker = static_cast<PahoWorker *>(context);
        const int actualTopicLength =
            topicLength > 0 ? topicLength
                            : static_cast<int>(strlen(topicName));
        const QString topic =
            QString::fromUtf8(topicName, actualTopicLength);
        const QByteArray payload(
            static_cast<const char *>(message->payload),
            message->payloadlen);
        emit worker->message(topic, payload);
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }

    AppSettings m_settings;
    MQTTClient m_client = nullptr;
    std::atomic<bool> m_connected{false};
};

class PahoMqttTransport final : public MqttTransport
{
    Q_OBJECT

public:
    explicit PahoMqttTransport(QObject *parent = nullptr)
        : MqttTransport(parent),
          m_worker(new PahoWorker),
          m_thread(new QThread(this))
    {
        m_worker->moveToThread(m_thread);
        connect(m_thread, &QThread::finished,
                m_worker, &QObject::deleteLater);
        connect(this, &PahoMqttTransport::requestConnect,
                m_worker, &PahoWorker::connectBroker);
        connect(this, &PahoMqttTransport::requestDisconnect,
                m_worker, &PahoWorker::disconnectBroker);
        connect(this, &PahoMqttTransport::requestPublish,
                m_worker, &PahoWorker::publish);
        connect(m_worker, &PahoWorker::message,
                this, &MqttTransport::messageReceived);
        connect(m_worker, &PahoWorker::publishOk,
                this, &MqttTransport::publishCompleted);
        connect(m_worker, &PahoWorker::publishError,
                this, &MqttTransport::publishFailed);
        connect(m_worker, &PahoWorker::connectionResult,
                this, &PahoMqttTransport::handleConnectionResult);

        m_retryTimer.setSingleShot(true);
        connect(&m_retryTimer, &QTimer::timeout,
                this, &PahoMqttTransport::attemptConnect);
        m_thread->start();
    }

    ~PahoMqttTransport() override
    {
        m_desiredConnected = false;
        m_retryTimer.stop();
        if (m_thread->isRunning()) {
            QMetaObject::invokeMethod(m_worker, "shutdown",
                                      Qt::BlockingQueuedConnection);
            m_thread->quit();
            m_thread->wait(3000);
        }
    }

    QString backendName() const override
    {
        return tr("Paho MQTT C");
    }

    bool backendAvailable() const override
    {
        return true;
    }

public slots:
    void connectToBroker(const AppSettings &settings) override
    {
        m_settings = settings;
        m_desiredConnected = true;
        m_retryDelaySeconds = 1;
        m_retryTimer.stop();
        attemptConnect();
    }

    void disconnectFromBroker() override
    {
        m_desiredConnected = false;
        m_retryTimer.stop();
        emit requestDisconnect();
    }

    void publishMessage(const QString &topic,
                        const QByteArray &payload,
                        int qos,
                        bool retained) override
    {
        emit requestPublish(topic, payload, qos, retained);
    }

signals:
    void requestConnect(const AppSettings &settings);
    void requestDisconnect();
    void requestPublish(const QString &topic,
                        const QByteArray &payload,
                        int qos,
                        bool retained);

private slots:
    void attemptConnect()
    {
        if (!m_desiredConnected) {
            return;
        }
        emit logMessage(tr("Connecting to %1 ...").arg(m_settings.brokerUri()));
        emit requestConnect(m_settings);
    }

    void handleConnectionResult(bool connected, const QString &detail)
    {
        emit connectionChanged(connected, detail);
        emit logMessage(detail);
        if (connected) {
            m_retryDelaySeconds = 1;
            return;
        }
        if (!m_desiredConnected || m_retryTimer.isActive()) {
            return;
        }
        emit logMessage(
            tr("Retrying in %1 seconds.").arg(m_retryDelaySeconds));
        m_retryTimer.start(m_retryDelaySeconds * 1000);
        m_retryDelaySeconds = qMin(m_retryDelaySeconds * 2, 30);
    }

private:
    AppSettings m_settings;
    PahoWorker *m_worker;
    QThread *m_thread;
    QTimer m_retryTimer;
    bool m_desiredConnected = false;
    int m_retryDelaySeconds = 1;
};

#endif // MQTT_DASHBOARD_WITH_PAHO

} // namespace

MqttTransport *createMqttTransport(bool demoMode, QObject *parent)
{
    if (demoMode) {
        return new DemoMqttTransport(parent);
    }
#ifdef MQTT_DASHBOARD_WITH_PAHO
    return new PahoMqttTransport(parent);
#else
    return new UnavailableMqttTransport(parent);
#endif
}

bool pahoBackendCompiled()
{
#ifdef MQTT_DASHBOARD_WITH_PAHO
    return true;
#else
    return false;
#endif
}

#include "mqtttransport.moc"
