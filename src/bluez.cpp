#include "bluez.h"

#include <QDebug>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QVariantMap>

#include "bluezutil.h"

namespace {
#define ORG_BLUEZ "org.bluez"
#define IFACE_MANAGER "org.bluez.Manager1"
#define IFACE_ADAPTER "org.bluez.Adapter1"
#define IFACE_DEVICE "org.bluez.Device1"
#define IFACE_GATT_DEVICE "org.bluez.GattDevice1"
#define IFACE_GATT_SERVICE "org.bluez.GattService1"
#define IFACE_GATT_CHAR "org.bluez.GattCharacteristic1"
#define IFACE_PROPS "org.freedesktop.DBus.Properties"
#define IFACE_OBJECT_MANAGER "org.freedesktop.DBus.ObjectManager"

constexpr int DISCOVERY_SETTLE_MS = 2000;
} // namespace

QString Bluez::defaultAdapterPath()
{
    const bluez::Reply reply = bluez::call(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), QStringLiteral("/"),
        IFACE_MANAGER, QStringLiteral("Get"),
        {QVariant(QStringLiteral(ORG_BLUEZ)), QVariant(QStringLiteral("DefaultAdapter"))});
    if (reply.ok) {
        const QString path = reply.value.toString();
        if (!path.isEmpty()) {
            return path;
        }
    }
    const bluez::Reply adapters = bluez::call(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), QStringLiteral("/"),
        IFACE_MANAGER, QStringLiteral("Get"),
        {QVariant(QStringLiteral(ORG_BLUEZ)), QVariant(QStringLiteral("Adapters"))});
    if (adapters.ok) {
        const QStringList list = adapters.value.toStringList();
        if (!list.isEmpty()) {
            return list.first();
        }
    }
    return QString();
}

bool Bluez::adapterPowered(const QString &adapterPath)
{
    if (adapterPath.isEmpty()) {
        return false;
    }
    const bluez::Reply reply = bluez::call(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), adapterPath,
        IFACE_PROPS, QStringLiteral("Get"),
        {QVariant(QStringLiteral(IFACE_ADAPTER)), QVariant(QStringLiteral("Powered"))});
    return reply.ok && reply.value.toBool();
}

BluezDevice::BluezDevice(const QString &objectPath, QObject *parent)
    : QObject(parent), m_objectPath(objectPath)
{
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(DISCOVERY_SETTLE_MS);
    connect(&m_settleTimer, &QTimer::timeout, this, [this] {
        if (m_gattConnected && !m_discoveryDone) {
            m_discoveryDone = true;
            Q_EMIT discoveryFinished();
        }
    });

    QDBusConnection bus = QDBusConnection::systemBus();

    // Device1 property updates (name, connection state) on our object path.
    if (!bus.connect(QStringLiteral(ORG_BLUEZ), m_objectPath, IFACE_PROPS,
                     QStringLiteral("PropertiesChanged"), this,
                     SLOT(onDevicePropertiesChanged(QString, QVariantMap, QStringList)))) {
        qWarning() << "BlueZ: could not connect to Device1 PropertiesChanged for" << m_objectPath;
    }
    // GATT service/characteristic objects appear and disappear via ObjectManager.
    if (!bus.connect(QStringLiteral(ORG_BLUEZ), QStringLiteral("/"), IFACE_OBJECT_MANAGER,
                     QStringLiteral("InterfacesAdded"), this,
                     SLOT(onInterfacesAdded(QString, QVariantMap)))) {
        qWarning() << "BlueZ: could not connect to ObjectManager InterfacesAdded for" << m_objectPath;
    }
    if (!bus.connect(QStringLiteral(ORG_BLUEZ), QStringLiteral("/"), IFACE_OBJECT_MANAGER,
                     QStringLiteral("InterfacesRemoved"), this,
                     SLOT(onInterfacesRemoved(QString, QStringList)))) {
        qWarning() << "BlueZ: could not connect to ObjectManager InterfacesRemoved for" << m_objectPath;
    }

    const bluez::Reply address = bluez::call(
        bus, QStringLiteral(ORG_BLUEZ), m_objectPath, IFACE_PROPS, QStringLiteral("Get"),
        {QVariant(QStringLiteral(IFACE_DEVICE)), QVariant(QStringLiteral("Address"))});
    if (address.ok) {
        m_address = address.value.toString();
    }
    updateName();
}

void BluezDevice::updateName()
{
    QDBusConnection bus = QDBusConnection::systemBus();
    const bluez::Reply name = bluez::call(
        bus, QStringLiteral(ORG_BLUEZ), m_objectPath, IFACE_PROPS, QStringLiteral("Get"),
        {QVariant(QStringLiteral(IFACE_DEVICE)), QVariant(QStringLiteral("Name"))});
    if (name.ok) {
        const QString value = name.value.toString();
        if (!value.isEmpty() && value != m_name) {
            m_name = value;
            Q_EMIT nameChanged(m_name);
            return;
        }
    }
    const bluez::Reply alias = bluez::call(
        bus, QStringLiteral(ORG_BLUEZ), m_objectPath, IFACE_PROPS, QStringLiteral("Get"),
        {QVariant(QStringLiteral(IFACE_DEVICE)), QVariant(QStringLiteral("Alias"))});
    if (alias.ok) {
        const QString value = alias.value.toString();
        if (!value.isEmpty() && value != m_name) {
            m_name = value;
            Q_EMIT nameChanged(m_name);
        }
    }
}

void BluezDevice::connectGatt()
{
    if (m_gattConnected || m_connectRequested) {
        return;
    }
    m_connectRequested = true;
    const bluez::Reply reply = bluez::call(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), m_objectPath,
        IFACE_GATT_DEVICE, QStringLiteral("Connect"));
    if (!reply.ok && !reply.noReply) {
        qWarning() << "BlueZ GattDevice1.Connect failed:" << reply.error;
        m_connectRequested = false;
    }
    // The real state arrives through the Device1 "Connected" property.
}

void BluezDevice::disconnectGatt()
{
    if (!m_gattConnected && !m_connectRequested) {
        return;
    }
    m_connectRequested = false;
    const bluez::Reply reply = bluez::call(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), m_objectPath,
        IFACE_GATT_DEVICE, QStringLiteral("Disconnect"));
    if (!reply.ok && !reply.noReply) {
        qWarning() << "BlueZ GattDevice1.Disconnect failed:" << reply.error;
    }
}

void BluezDevice::setGattConnected(bool connected)
{
    if (m_gattConnected == connected) {
        return;
    }
    m_gattConnected = connected;
    if (!connected) {
        m_connectRequested = false;
        m_discoveryDone = false;
        m_settleTimer.stop();
        m_servicePathsByUuid.clear();
        m_charPathsByUuid.clear();
        m_charServiceByUuid.clear();
    }
    Q_EMIT gattConnectedChanged(m_gattConnected);
}

void BluezDevice::requestServiceDiscovery()
{
    if (m_discoveryDone) {
        return;
    }
    // Services may already be indexed (BlueZ caches the GATT DB across
    // connections for some devices).
    indexManagedObjects();
    if (m_charPathsByUuid.isEmpty()) {
        const bluez::Reply reply = bluez::call(
            QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), m_objectPath,
            IFACE_GATT_DEVICE, QStringLiteral("RequestServiceDiscovery"));
        if (!reply.ok) {
            // No GATT on this device; finish with what we have.
            if (!reply.noReply) {
                qWarning() << "BlueZ RequestServiceDiscovery failed:" << reply.error;
            }
            m_discoveryDone = true;
            Q_EMIT discoveryFinished();
            return;
        }
    }
    m_settleTimer.start();
}

void BluezDevice::indexManagedObjects()
{
    const bluez::Reply reply = bluez::call(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), QStringLiteral("/"),
        IFACE_OBJECT_MANAGER, QStringLiteral("GetManagedObjects"));
    if (!reply.ok) {
        qWarning() << "BlueZ GetManagedObjects failed:" << reply.error;
        return;
    }
    const QVariantMap managed = reply.value.toMap();
    for (auto it = managed.constBegin(); it != managed.constEnd(); ++it) {
        const QString path = it.key();
        if (path != m_objectPath && !path.startsWith(m_objectPath + QLatin1Char('/'))) {
            continue;
        }
        addInterfaceObjects(path, it.value().toMap());
    }
}

void BluezDevice::onDevicePropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    if (iface != QLatin1String(IFACE_DEVICE)) {
        return;
    }
    if (changed.contains(QStringLiteral("Name")) || changed.contains(QStringLiteral("Alias"))) {
        updateName();
    }
    if (changed.contains(QStringLiteral("Connected"))) {
        if (changed.value(QStringLiteral("Connected")).toBool()) {
            setGattConnected(true);
            requestServiceDiscovery();
        } else {
            setGattConnected(false);
        }
    }
}

void BluezDevice::onInterfacesAdded(const QString &path, const QVariantMap &interfaces)
{
    if (path != m_objectPath && !path.startsWith(m_objectPath + QLatin1Char('/'))) {
        return;
    }
    if (!interfaces.contains(QLatin1String(IFACE_GATT_SERVICE))
            && !interfaces.contains(QLatin1String(IFACE_GATT_CHAR))) {
        return;
    }
    addInterfaceObjects(path, interfaces);
    if (m_gattConnected && !m_discoveryDone) {
        m_settleTimer.start();
    }
}

void BluezDevice::onInterfacesRemoved(const QString &path, const QStringList &interfaces)
{
    Q_UNUSED(interfaces)
    if (path != m_objectPath && !path.startsWith(m_objectPath + QLatin1Char('/'))) {
        return;
    }
    for (auto it = m_servicePathsByUuid.begin(); it != m_servicePathsByUuid.end();) {
        if (it.value() == path) {
            it = m_servicePathsByUuid.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_charPathsByUuid.begin(); it != m_charPathsByUuid.end();) {
        if (it.value() == path) {
            it = m_charPathsByUuid.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = m_charServiceByUuid.begin(); it != m_charServiceByUuid.end();) {
        if (it.value() == path) {
            it = m_charServiceByUuid.erase(it);
        } else {
            ++it;
        }
    }
}

void BluezDevice::addInterfaceObjects(const QString &path, const QVariantMap &interfaces)
{
    const auto serviceIt = interfaces.find(QLatin1String(IFACE_GATT_SERVICE));
    if (serviceIt != interfaces.end()) {
        const QString uuid = serviceIt.value().toMap().value(QStringLiteral("UUID")).toString();
        if (!uuid.isEmpty()) {
            m_servicePathsByUuid.insert(uuid, path);
        }
    }
    const auto charIt = interfaces.find(QLatin1String(IFACE_GATT_CHAR));
    if (charIt != interfaces.end()) {
        const QVariantMap props = charIt.value().toMap();
        const QString uuid = props.value(QStringLiteral("UUID")).toString();
        if (!uuid.isEmpty()) {
            m_charPathsByUuid.insert(uuid, path);
            const QString servicePath = props.value(QStringLiteral("Service")).toString();
            if (!servicePath.isEmpty()) {
                m_charServiceByUuid.insert(uuid, servicePath);
            }
        }
    }
}

QString BluezDevice::characteristicPathInService(const QString &serviceUuid, const QString &uuid) const
{
    const QString charPath = m_charPathsByUuid.value(uuid);
    if (charPath.isEmpty()) {
        return QString();
    }
    const QString servicePath = m_charServiceByUuid.value(uuid);
    const QString wanted = m_servicePathsByUuid.value(serviceUuid);
    if (servicePath.isEmpty() || wanted.isEmpty() || servicePath != wanted) {
        return QString();
    }
    return charPath;
}

void BluezDevice::readCharacteristic(const QString &uuid)
{
    const QString charPath = m_charPathsByUuid.value(uuid);
    if (charPath.isEmpty() || !m_gattConnected) {
        return;
    }
    const QDBusPendingCall call = bluez::asyncCall(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), charPath,
        IFACE_GATT_CHAR, QStringLiteral("ReadValue"), {QVariant(QVariantMap())});
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, uuid, watcher](QDBusPendingCallWatcher *self) {
        watcher->deleteLater();
        if (self->isError()) {
            if (self->error().type() != QDBusError::NoReply) {
                qWarning() << "BlueZ ReadValue failed for" << uuid << ":" << self->error().message();
            }
            return;
        }
        const QVariantList args = self->reply().arguments();
        if (args.isEmpty()) {
            return;
        }
        Q_EMIT characteristicRead(uuid, args.at(0).toByteArray());
    });
}

void BluezDevice::writeCharacteristic(const QString &uuid, const QByteArray &value, bool withoutResponse)
{
    const QString charPath = m_charPathsByUuid.value(uuid);
    if (charPath.isEmpty() || !m_gattConnected) {
        return;
    }
    writeCharacteristicPath(charPath, value, withoutResponse);
}

void BluezDevice::writeCharacteristicPath(const QString &charPath, const QByteArray &value, bool withoutResponse)
{
    if (charPath.isEmpty() || !m_gattConnected) {
        return;
    }
    QVariantMap options;
    if (withoutResponse) {
        options.insert(QStringLiteral("without-response"), true);
    }
    doWrite(charPath, value, options, false);
}

void BluezDevice::doWrite(const QString &charPath, const QByteArray &value, const QVariantMap &options,
                          bool isRetry)
{
    const QDBusPendingCall call = bluez::asyncCall(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), charPath,
        IFACE_GATT_CHAR, QStringLiteral("WriteValue"),
        {QVariant(value), QVariant(options)});
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, charPath, value, isRetry, watcher](QDBusPendingCallWatcher *self) {
        watcher->deleteLater();
        if (!self->isError()) {
            return;
        }
        if (isRetry) {
            qWarning() << "BlueZ WriteValue failed:" << self->error().message();
            return;
        }
        // Retry once with plain options in case the option set is rejected.
        qWarning() << "BlueZ WriteValue with options failed, retrying:" << self->error().message();
        doWrite(charPath, value, QVariantMap(), true);
    });
}
