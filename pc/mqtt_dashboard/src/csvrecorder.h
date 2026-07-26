#ifndef CSVRECORDER_H
#define CSVRECORDER_H

#include "telemetry.h"

#include <QObject>
#include <QString>

class CsvRecorder : public QObject
{
    Q_OBJECT

public:
    explicit CsvRecorder(QObject *parent = nullptr);

    void configure(bool enabled, const QString &directory);
    bool isEnabled() const;
    QString currentFilePath() const;
    bool record(const TelemetrySample &sample);

signals:
    void errorOccurred(const QString &message);
    void fileChanged(const QString &path);

private:
    QString csvEscape(const QString &value) const;
    QString filePathFor(const QDate &date) const;

    bool m_enabled = false;
    QString m_directory;
    QString m_currentFilePath;
};

#endif
