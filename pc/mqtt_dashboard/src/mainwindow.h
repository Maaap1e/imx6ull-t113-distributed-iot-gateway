#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "appsettings.h"

#include <QDateTime>
#include <QMainWindow>

class ControlPage;
class CsvRecorder;
class MqttTransport;
class OverviewPage;
class QButtonGroup;
class QLabel;
class QPushButton;
class QStackedWidget;
class QTimer;
class QTranslator;
class SettingsPage;
class TrendsPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void toggleConnection();
    void handleConnectionChanged(bool connected, const QString &detail);
    void handleMessage(const QString &topic, const QByteArray &payload);
    void handleLedCommand(int value);
    void handleSettingsApplied(const AppSettings &settings);
    void checkTelemetryFreshness();
    void showPage(int index);

private:
    void buildUi();
    void recreateTransport();
    void connectTransportSignals();
    void requestConnection();
    void applyLanguage(const QString &language);
    void loadStyleSheet();
    void setHeaderState(bool online, const QString &detail);
    void setFooterMessage(const QString &message, bool error = false);
    void updateCounters();
    void retranslateUi();

    AppSettings m_settings;
    MqttTransport *m_transport = nullptr;
    CsvRecorder *m_csvRecorder = nullptr;
    QTranslator *m_translator = nullptr;

    OverviewPage *m_overviewPage = nullptr;
    TrendsPage *m_trendsPage = nullptr;
    ControlPage *m_controlPage = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    QStackedWidget *m_pages = nullptr;
    QButtonGroup *m_navigation = nullptr;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_brokerLabel = nullptr;
    QLabel *m_connectionBadge = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_overviewButton = nullptr;
    QPushButton *m_trendsButton = nullptr;
    QPushButton *m_controlButton = nullptr;
    QPushButton *m_settingsButton = nullptr;
    QLabel *m_footerMessage = nullptr;
    QLabel *m_counterLabel = nullptr;
    QLabel *m_csvLabel = nullptr;
    QTimer *m_freshnessTimer = nullptr;

    QDateTime m_lastTelemetry;
    quint64 m_receivedMessages = 0;
    quint64 m_parseErrors = 0;
    bool m_connected = false;
    bool m_connectionRequested = false;
};

#endif
