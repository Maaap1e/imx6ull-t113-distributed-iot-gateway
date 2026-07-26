QT += core testlib
CONFIG += console c++11 testcase
TEMPLATE = app
TARGET = mqtt_dashboard_parser_test

INCLUDEPATH += ../src

SOURCES += \
    test_telemetryparser.cpp \
    ../src/telemetryparser.cpp

HEADERS += \
    ../src/telemetryparser.h \
    ../src/telemetry.h
