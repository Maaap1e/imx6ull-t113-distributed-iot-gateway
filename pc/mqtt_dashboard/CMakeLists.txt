cmake_minimum_required(VERSION 3.16)

project(IoTGatewayMqttDashboard VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

option(MQTT_DASHBOARD_WITH_PAHO
       "Build the real MQTT backend with Eclipse Paho MQTT C" OFF)
set(PAHO_ROOT "" CACHE PATH "Windows Eclipse Paho MQTT C install prefix")

find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets Charts LinguistTools)

set(DASHBOARD_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/appsettings.cpp
    src/appsettings.h
    src/telemetry.h
    src/telemetryparser.cpp
    src/telemetryparser.h
    src/csvrecorder.cpp
    src/csvrecorder.h
    src/mqtttransport.cpp
    src/mqtttransport.h
    src/overviewpage.cpp
    src/overviewpage.h
    src/trendspage.cpp
    src/trendspage.h
    src/controlpage.cpp
    src/controlpage.h
    src/settingspage.cpp
    src/settingspage.h
    resources/resources.qrc
)

qt5_add_translation(DASHBOARD_QM_FILES
    i18n/mqtt_dashboard_zh_CN.ts
)

add_executable(mqtt_dashboard WIN32
    ${DASHBOARD_SOURCES}
    ${DASHBOARD_QM_FILES}
)
target_include_directories(mqtt_dashboard PRIVATE src)
target_link_libraries(mqtt_dashboard PRIVATE
    Qt5::Widgets
    Qt5::Charts
)
target_compile_definitions(mqtt_dashboard PRIVATE QT_DEPRECATED_WARNINGS)

if(MQTT_DASHBOARD_WITH_PAHO)
    find_path(PAHO_INCLUDE_DIR MQTTClient.h
        HINTS "${PAHO_ROOT}/include")
    find_library(PAHO_LIBRARY
        NAMES paho-mqtt3c libpaho-mqtt3c
        HINTS "${PAHO_ROOT}/lib")
    if(NOT PAHO_INCLUDE_DIR OR NOT PAHO_LIBRARY)
        message(FATAL_ERROR
            "Paho MQTT C was requested but MQTTClient.h/library was not found")
    endif()
    target_compile_definitions(mqtt_dashboard PRIVATE
        MQTT_DASHBOARD_WITH_PAHO)
    target_include_directories(mqtt_dashboard PRIVATE ${PAHO_INCLUDE_DIR})
    target_link_libraries(mqtt_dashboard PRIVATE ${PAHO_LIBRARY})
endif()

add_custom_command(TARGET mqtt_dashboard POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:mqtt_dashboard>/translations"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${DASHBOARD_QM_FILES}
            "$<TARGET_FILE_DIR:mqtt_dashboard>/translations"
    VERBATIM
)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt5 5.12 REQUIRED COMPONENTS Test)
    add_executable(mqtt_dashboard_parser_test
        tests/test_telemetryparser.cpp
        src/telemetryparser.cpp
        src/telemetryparser.h
        src/telemetry.h
    )
    target_include_directories(mqtt_dashboard_parser_test PRIVATE src)
    target_link_libraries(mqtt_dashboard_parser_test PRIVATE
        Qt5::Core
        Qt5::Test
    )
    add_test(NAME mqtt_dashboard_parser_test
             COMMAND mqtt_dashboard_parser_test)
endif()
