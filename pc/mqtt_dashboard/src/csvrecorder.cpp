#include "csvrecorder.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

CsvRecorder::CsvRecorder(QObject *parent)
    : QObject(parent)
{
}

void CsvRecorder::configure(bool enabled, const QString &directory)
{
    m_enabled = enabled;
    m_directory = QDir::cleanPath(directory.trimmed());
    m_currentFilePath.clear();
}

bool CsvRecorder::isEnabled() const
{
    return m_enabled;
}

QString CsvRecorder::currentFilePath() const
{
    return m_currentFilePath;
}

QString CsvRecorder::filePathFor(const QDate &date) const
{
    return QDir(m_directory)
        .filePath(QStringLiteral("telemetry_%1.csv")
                      .arg(date.toString(QStringLiteral("yyyyMMdd"))));
}

QString CsvRecorder::csvEscape(const QString &value) const
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QStringLiteral("\"%1\"").arg(escaped);
}

bool CsvRecorder::record(const TelemetrySample &sample)
{
    if (!m_enabled) {
        return true;
    }
    if (m_directory.isEmpty()) {
        emit errorOccurred(tr("CSV directory is empty."));
        return false;
    }
    QDir directory;
    if (!directory.mkpath(m_directory)) {
        emit errorOccurred(
            tr("Cannot create CSV directory: %1").arg(m_directory));
        return false;
    }

    const QString path = filePathFor(sample.receivedAt.date());
    const QFileInfo info(path);
    const bool writeHeader = !info.exists() || info.size() == 0;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append |
                   QIODevice::Text)) {
        emit errorOccurred(
            tr("Cannot open CSV file: %1").arg(file.errorString()));
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    if (writeHeader) {
        // UTF-8 BOM keeps Chinese paths and future text columns readable in
        // common Windows spreadsheet applications.
        stream << QChar(0xFEFF);
        stream
            << "received_time_iso,device_timestamp,simulated,tcp_connected,"
               "tcp_reconnect_count,tcp_disconnect_count,ap3216c_ok,als,ir,ps,"
               "icm20608_ok,gyro_x,gyro_y,gyro_z,accel_x,accel_y,accel_z,"
               "imu_temperature,stm32_can_ok,stm32_online,dht_ok,"
               "temperature,humidity,app_version,counter,frames,"
               "checksum_errors,stm32_last_seen\n";
    }

    stream << csvEscape(sample.receivedAt.toString(Qt::ISODateWithMs)) << ','
           << sample.deviceTimestamp << ','
           << (sample.simulated ? 1 : 0) << ','
           << (sample.tcpConnected ? 1 : 0) << ','
           << sample.tcpReconnectCount << ','
           << sample.tcpDisconnectCount << ','
           << (sample.ap3216cOk ? 1 : 0) << ','
           << sample.als << ',' << sample.ir << ',' << sample.ps << ','
           << (sample.icm20608Ok ? 1 : 0) << ','
           << QString::number(sample.gyroX, 'f', 3) << ','
           << QString::number(sample.gyroY, 'f', 3) << ','
           << QString::number(sample.gyroZ, 'f', 3) << ','
           << QString::number(sample.accelX, 'f', 3) << ','
           << QString::number(sample.accelY, 'f', 3) << ','
           << QString::number(sample.accelZ, 'f', 3) << ','
           << QString::number(sample.imuTemperature, 'f', 2) << ','
           << (sample.stm32CanOk ? 1 : 0) << ','
           << (sample.stm32Online ? 1 : 0) << ','
           << (sample.dhtOk ? 1 : 0) << ','
           << sample.temperature << ',' << sample.humidity << ','
           << csvEscape(sample.appVersion) << ','
           << sample.counter << ',' << sample.frames << ','
           << sample.checksumErrors << ',' << sample.stm32LastSeen << '\n';
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        emit errorOccurred(tr("Failed to write CSV data."));
        return false;
    }
    if (m_currentFilePath != path) {
        m_currentFilePath = path;
        emit fileChanged(path);
    }
    return true;
}
