QT += core gui widgets charts network
CONFIG += c++11 warn_on
TEMPLATE = app
TARGET = mqtt_dashboard

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD/src

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/appsettings.cpp \
    src/telemetryparser.cpp \
    src/csvrecorder.cpp \
    src/mqtttransport.cpp \
    src/overviewpage.cpp \
    src/trendspage.cpp \
    src/controlpage.cpp \
    src/settingspage.cpp

HEADERS += \
    src/mainwindow.h \
    src/appsettings.h \
    src/telemetry.h \
    src/telemetryparser.h \
    src/csvrecorder.h \
    src/mqtttransport.h \
    src/overviewpage.h \
    src/trendspage.h \
    src/controlpage.h \
    src/settingspage.h

RESOURCES += resources/resources.qrc

TRANSLATIONS += i18n/mqtt_dashboard_zh_CN.ts
CONFIG += lrelease embed_translations
QM_FILES_RESOURCE_PREFIX = /i18n

# Add "CONFIG+=paho PAHO_ROOT=C:/path/to/paho" to qmake when a Windows
# Eclipse Paho MQTT C development package is available. Without it, the
# application remains fully buildable and uses the explicit Demo mode.
contains(CONFIG, paho) {
    isEmpty(PAHO_ROOT) {
        error("CONFIG+=paho requires PAHO_ROOT=/path/to/paho")
    }
    DEFINES += MQTT_DASHBOARD_WITH_PAHO
    INCLUDEPATH += $$PAHO_ROOT/include
    LIBS += -L$$PAHO_ROOT/lib -lpaho-mqtt3c
}

win32 {
    RC_ICONS =
}
