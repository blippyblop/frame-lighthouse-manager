#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QUrl>

#include "appmodel.h"
#include "blemanager.h"
#include "settingsstore.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("lighthouse-pm-kde"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("lighthouse-pm-kde.local"));
    QCoreApplication::setApplicationName(QStringLiteral("lighthouse-pm"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    auto *bleManager = new BleManager(&app);
    auto *settings = &bleManager->settings();
    auto *model = new AppModel(bleManager, settings, &app);

    QQmlEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("bleManager"), bleManager);
    engine.rootContext()->setContextProperty(QStringLiteral("settingsStore"), settings);
    engine.rootContext()->setContextProperty(QStringLiteral("appModel"), model);

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qt/qml/lighthousepm/src/qml/main.qml")));
    if (component.isError()) {
        qWarning() << "QML load error:" << component.errorString();
        qWarning() << component.errors();
        return 1;
    }

    QObject *window = component.create();
    if (!window) {
        qWarning() << "Failed to create main window";
        return 1;
    }

    return app.exec();
}
