#ifndef TRENDSPAGE_H
#define TRENDSPAGE_H

#include "telemetry.h"

#include <QHash>
#include <QPointF>
#include <QVector>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QPushButton;
QT_END_NAMESPACE

namespace QtCharts {
class QChart;
class QChartView;
class QDateTimeAxis;
class QLineSeries;
class QValueAxis;
}

class TrendsPage : public QWidget
{
    Q_OBJECT

public:
    explicit TrendsPage(QWidget *parent = nullptr);

    void appendTelemetry(const TelemetrySample &sample);

protected:
    void changeEvent(QEvent *event) override;

private slots:
    void refreshChart();
    void clearHistory();
    void togglePause();

private:
    void addPoint(const QString &key, qint64 timestampMs, double value);
    QString selectedMetricKey() const;
    void retranslateUi();

    QLabel *m_pageTitle = nullptr;
    QLabel *m_pageSubtitle = nullptr;
    QLabel *m_metricLabel = nullptr;
    QLabel *m_windowLabel = nullptr;
    QLabel *m_sampleCount = nullptr;
    QComboBox *m_metricCombo = nullptr;
    QComboBox *m_windowCombo = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_clearButton = nullptr;

    QtCharts::QChart *m_chart = nullptr;
    QtCharts::QChartView *m_chartView = nullptr;
    QtCharts::QLineSeries *m_series = nullptr;
    QtCharts::QDateTimeAxis *m_timeAxis = nullptr;
    QtCharts::QValueAxis *m_valueAxis = nullptr;

    QHash<QString, QVector<QPointF>> m_history;
    bool m_paused = false;
};

#endif
