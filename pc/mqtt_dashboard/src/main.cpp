#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QMetaObject>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("Maaap1e"));
    QCoreApplication::setApplicationName(
        QStringLiteral("IoTGatewayMqttDashboard"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.1.0"));
    QApplication::setStyle(QStringLiteral("Fusion"));

    MainWindow window;
    window.show();

    const QStringList arguments = application.arguments();
    const bool smokeTest = arguments.contains(QStringLiteral("--smoke-test"));
    QString screenshotPath;
    int requestedPage = -1;
    for (const QString &argument : arguments) {
        if (argument.startsWith(QStringLiteral("--screenshot="))) {
            screenshotPath = argument.mid(QStringLiteral("--screenshot=").size());
        } else if (argument.startsWith(QStringLiteral("--page="))) {
            bool ok = false;
            const int value =
                argument.mid(QStringLiteral("--page=").size()).toInt(&ok);
            if (ok && value >= 0 && value <= 3) {
                requestedPage = value;
            }
        }
    }
    if (requestedPage >= 0) {
        QMetaObject::invokeMethod(&window, "showPage",
                                  Qt::DirectConnection,
                                  Q_ARG(int, requestedPage));
    }
    if (smokeTest || !screenshotPath.isEmpty()) {
        QTimer::singleShot(3000, &application,
                           [&application, &window, screenshotPath]() {
            if (!screenshotPath.isEmpty()) {
                const QString absolutePath =
                    QDir::current().absoluteFilePath(screenshotPath);
                window.grab().save(absolutePath);
            }
            application.quit();
        });
    }
    return application.exec();
}
