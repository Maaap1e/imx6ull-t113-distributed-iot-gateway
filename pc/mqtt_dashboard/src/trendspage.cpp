#include "trendspage.h"

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include <QComboBox>
#include <QColor>
#include <QDateTime>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <limits>

TrendsPage::TrendsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 24);
    root->setSpacing(16);

    m_pageTitle = new QLabel(this);
    m_pageTitle->setObjectName(QStringLiteral("pageTitle"));
    m_pageSubtitle = new QLabel(this);
    m_pageSubtitle->setObjectName(QStringLiteral("pageSubtitle"));
    root->addWidget(m_pageTitle);
    root->addWidget(m_pageSubtitle);

    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(10);
    m_metricLabel = new QLabel(this);
    m_metricCombo = new QComboBox(this);
    m_metricCombo->setMinimumWidth(180);
    m_windowLabel = new QLabel(this);
    m_windowCombo = new QComboBox(this);
    m_windowCombo->setMinimumWidth(100);
    m_pauseButton = new QPushButton(this);
    m_clearButton = new QPushButton(this);
    m_sampleCount = new QLabel(this);
    m_sampleCount->setObjectName(QStringLiteral("mutedLabel"));

    toolbar->addWidget(m_metricLabel);
    toolbar->addWidget(m_metricCombo);
    toolbar->addSpacing(12);
    toolbar->addWidget(m_windowLabel);
    toolbar->addWidget(m_windowCombo);
    toolbar->addWidget(m_pauseButton);
    toolbar->addWidget(m_clearButton);
    toolbar->addStretch();
    toolbar->addWidget(m_sampleCount);
    root->addLayout(toolbar);

    m_series = new QtCharts::QLineSeries(this);
    m_series->setUseOpenGL(false);
    m_series->setPen(QPen(QColor(QStringLiteral("#35a7ff")), 2.0));
    m_chart = new QtCharts::QChart;
    m_chart->setTheme(QtCharts::QChart::ChartThemeDark);
    m_chart->addSeries(m_series);
    m_chart->legend()->hide();
    m_chart->setBackgroundBrush(QColor(QStringLiteral("#111b2e")));
    m_chart->setBackgroundRoundness(8.0);
    m_chart->setPlotAreaBackgroundBrush(
        QColor(QStringLiteral("#0c1525")));
    m_chart->setPlotAreaBackgroundVisible(true);
    m_chart->setTitleBrush(QColor(QStringLiteral("#dce6f2")));

    m_timeAxis = new QtCharts::QDateTimeAxis(this);
    m_timeAxis->setFormat(QStringLiteral("HH:mm:ss"));
    m_timeAxis->setTickCount(7);
    m_valueAxis = new QtCharts::QValueAxis(this);
    m_valueAxis->setLabelFormat(QStringLiteral("%.2f"));
    m_valueAxis->setTickCount(6);
    const QBrush axisText(QColor(QStringLiteral("#a9b9cd")));
    const QPen gridPen(QColor(QStringLiteral("#263651")));
    m_timeAxis->setLabelsBrush(axisText);
    m_timeAxis->setTitleBrush(axisText);
    m_timeAxis->setGridLinePen(gridPen);
    m_valueAxis->setLabelsBrush(axisText);
    m_valueAxis->setTitleBrush(axisText);
    m_valueAxis->setGridLinePen(gridPen);
    m_chart->addAxis(m_timeAxis, Qt::AlignBottom);
    m_chart->addAxis(m_valueAxis, Qt::AlignLeft);
    m_series->attachAxis(m_timeAxis);
    m_series->attachAxis(m_valueAxis);

    m_chartView = new QtCharts::QChartView(m_chart, this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setBackgroundBrush(
        QColor(QStringLiteral("#111b2e")));
    m_chartView->setFrameShape(QFrame::NoFrame);
    m_chartView->setObjectName(QStringLiteral("chartView"));
    root->addWidget(m_chartView, 1);

    connect(m_metricCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrendsPage::refreshChart);
    connect(m_windowCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrendsPage::refreshChart);
    connect(m_pauseButton, &QPushButton::clicked,
            this, &TrendsPage::togglePause);
    connect(m_clearButton, &QPushButton::clicked,
            this, &TrendsPage::clearHistory);

    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &TrendsPage::refreshChart);
    timer->start();

    retranslateUi();
    refreshChart();
}

void TrendsPage::addPoint(const QString &key,
                          qint64 timestampMs,
                          double value)
{
    QVector<QPointF> &values = m_history[key];
    values.append(QPointF(static_cast<qreal>(timestampMs), value));
    const int maximumSamples = 7200;
    if (values.size() > maximumSamples) {
        values.remove(0, values.size() - maximumSamples);
    }
}

void TrendsPage::appendTelemetry(const TelemetrySample &sample)
{
    const qint64 timestamp = sample.receivedAt.toMSecsSinceEpoch();
    addPoint(QStringLiteral("temperature"), timestamp, sample.temperature);
    addPoint(QStringLiteral("humidity"), timestamp, sample.humidity);
    addPoint(QStringLiteral("illuminance"), timestamp, sample.als);
    addPoint(QStringLiteral("proximity"), timestamp, sample.ps);
    addPoint(QStringLiteral("imu_temperature"), timestamp,
             sample.imuTemperature);
    if (!m_paused) {
        refreshChart();
    }
}

QString TrendsPage::selectedMetricKey() const
{
    return m_metricCombo->currentData().toString();
}

void TrendsPage::refreshChart()
{
    if (m_paused) {
        return;
    }

    const QString key = selectedMetricKey();
    const QVector<QPointF> values = m_history.value(key);
    const int windowMinutes = m_windowCombo->currentData().toInt();
    const qint64 endMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 startMs =
        endMs - static_cast<qint64>(windowMinutes) * 60 * 1000;

    QVector<QPointF> visible;
    visible.reserve(values.size());
    double minimum = std::numeric_limits<double>::max();
    double maximum = std::numeric_limits<double>::lowest();
    for (const QPointF &point : values) {
        if (point.x() < static_cast<qreal>(startMs)) {
            continue;
        }
        visible.append(point);
        minimum = qMin(minimum, point.y());
        maximum = qMax(maximum, point.y());
    }
    m_series->replace(visible);
    m_timeAxis->setRange(QDateTime::fromMSecsSinceEpoch(startMs),
                         QDateTime::fromMSecsSinceEpoch(endMs));

    if (visible.isEmpty()) {
        m_valueAxis->setRange(0.0, 1.0);
    } else {
        const double span = maximum - minimum;
        const double padding =
            span > 0.01 ? span * 0.12 : qMax(qAbs(maximum) * 0.1, 1.0);
        m_valueAxis->setRange(minimum - padding, maximum + padding);
    }
    m_sampleCount->setText(
        tr("%1 visible / %2 stored").arg(visible.size()).arg(values.size()));
}

void TrendsPage::clearHistory()
{
    m_history.clear();
    m_series->clear();
    refreshChart();
}

void TrendsPage::togglePause()
{
    m_paused = !m_paused;
    m_pauseButton->setText(m_paused ? tr("Resume") : tr("Pause"));
    if (!m_paused) {
        refreshChart();
    }
}

void TrendsPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    }
    QWidget::changeEvent(event);
}

void TrendsPage::retranslateUi()
{
    const QString metricKey = selectedMetricKey();
    const int windowValue = m_windowCombo->currentData().toInt();

    m_pageTitle->setText(tr("Telemetry Trends"));
    m_pageSubtitle->setText(
        tr("Charts use the PC receive time; the raw device epoch stays in CSV."));
    m_metricLabel->setText(tr("Metric"));
    m_windowLabel->setText(tr("Window"));
    m_pauseButton->setText(m_paused ? tr("Resume") : tr("Pause"));
    m_clearButton->setText(tr("Clear"));

    m_metricCombo->blockSignals(true);
    m_metricCombo->clear();
    m_metricCombo->addItem(tr("DHT11 temperature"),
                           QStringLiteral("temperature"));
    m_metricCombo->addItem(tr("DHT11 humidity"),
                           QStringLiteral("humidity"));
    m_metricCombo->addItem(tr("AP3216C illuminance"),
                           QStringLiteral("illuminance"));
    m_metricCombo->addItem(tr("AP3216C proximity"),
                           QStringLiteral("proximity"));
    m_metricCombo->addItem(tr("ICM20608 temperature"),
                           QStringLiteral("imu_temperature"));
    const int metricIndex = m_metricCombo->findData(metricKey);
    m_metricCombo->setCurrentIndex(metricIndex >= 0 ? metricIndex : 0);
    m_metricCombo->blockSignals(false);

    m_windowCombo->blockSignals(true);
    m_windowCombo->clear();
    m_windowCombo->addItem(tr("1 minute"), 1);
    m_windowCombo->addItem(tr("10 minutes"), 10);
    m_windowCombo->addItem(tr("30 minutes"), 30);
    m_windowCombo->addItem(tr("60 minutes"), 60);
    m_windowCombo->addItem(tr("3 hours"), 180);
    const int windowIndex = m_windowCombo->findData(windowValue);
    m_windowCombo->setCurrentIndex(windowIndex >= 0 ? windowIndex : 1);
    m_windowCombo->blockSignals(false);

    m_chart->setTitle(tr("Live telemetry"));
    m_timeAxis->setTitleText(tr("PC receive time"));
    m_valueAxis->setTitleText(tr("Value"));
    refreshChart();
}
