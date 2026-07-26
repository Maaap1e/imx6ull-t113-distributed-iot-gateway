#ifndef TELEMETRYPARSER_H
#define TELEMETRYPARSER_H

#include "telemetry.h"

#include <QByteArray>
#include <QString>

class TelemetryParser
{
public:
    static bool parseTelemetry(const QByteArray &payload,
                               TelemetrySample *sample,
                               QString *error);
    static bool parseCommandResponse(const QByteArray &payload,
                                     CommandResponse *response,
                                     QString *error);
    static bool parseOnlineStatus(const QByteArray &payload,
                                  bool *online,
                                  QString *reason,
                                  QString *error);
};

#endif
