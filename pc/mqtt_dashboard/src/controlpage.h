#ifndef CONTROLPAGE_H
#define CONTROLPAGE_H

#include "telemetry.h"

#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;

class ControlPage : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPage(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void beginCommand(int value);
    void handleResponse(const CommandResponse &response);
    void handlePublishFailure(const QString &error);

signals:
    void ledCommandRequested(int value);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void commandTimedOut();

private:
    void setButtonsEnabled(bool enabled);
    void appendHistory(const QString &text);
    QString commandName(int value) const;
    void retranslateUi();

    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageSubtitle = nullptr;
    QLabel *m_connectionHint = nullptr;
    QLabel *m_resultTitle = nullptr;
    QLabel *m_resultValue = nullptr;
    QLabel *m_historyTitle = nullptr;
    QPushButton *m_offButton = nullptr;
    QPushButton *m_onButton = nullptr;
    QPushButton *m_heartbeatButton = nullptr;
    QPlainTextEdit *m_history = nullptr;
    QTimer *m_timeout = nullptr;

    bool m_connected = false;
    int m_pendingValue = -1;
};

#endif
