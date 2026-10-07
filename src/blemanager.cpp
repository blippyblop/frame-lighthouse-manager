#include "blemanager.h"

#include <QDebug>
#include <QDBusConnection>
#include <QProcess>
#include <QVariantMap>
#include <utility>

#include "bluez.h"
#include "bluezutil.h"
#include "lighthousedevice.h"
#include "lighthousev2device.h"
#include "vivebasestation.h"

namespace {
#define ORG_BLUEZ "org.bluez"
#define IFACE_ADAPTER "org.bluez.Adapter1"
#define IFACE_DEVICE "org.bluez.Device1"
#define IFACE_PROPS "org.freedesktop.DBus.Properties"
#define IFACE_OBJECT_MANAGER "org.freedesktop.DBus.ObjectManager"

constexpr char LHB_PREFIX[] = "LHB-";
constexpr char VIVE_PREFIX[] = "HTC BS";
} // namespace

BleManager::BleManager(QObject *parent)
    : QObject(parent)
{
    m_adapterPath = Bluez::defaultAdapterPath();
    refreshAdapterState();

    QDBusConnection bus = QDBusConnection::systemBus();

    // Track the adapter powered state.
    if (!m_adapterPath.isEmpty()) {
        if (!bus.connect(QStringLiteral(ORG_BLUEZ), m_adapterPath, IFACE_PROPS,
                         QStringLiteral("PropertiesChanged"), this,
                         SLOT(onAdapterPropertiesChanged(QString, QVariantMap, QStringList)))) {
            qWarning() << "BlueZ: could not connect to adapter PropertiesChanged for" << m_adapterPath;
        }
    }

    // Device objects appear (scan results, pairing) via ObjectManager.
    if (!bus.connect(QStringLiteral(ORG_BLUEZ), QStringLiteral("/"), IFACE_OBJECT_MANAGER,
                     QStringLiteral("InterfacesAdded"), this,
                     SLOT(onManagerInterfacesAdded(QString, QVariantMap)))) {
        qWarning() << "BlueZ: could not connect to ObjectManager InterfacesAdded";
    }
    if (!bus.connect(QStringLiteral(ORG_BLUEZ), QStringLiteral("/"), IFACE_OBJECT_MANAGER,
                     QStringLiteral("InterfacesRemoved"), this,
                     SLOT(onManagerInterfacesRemoved(QString, QStringList)))) {
        qWarning() << "BlueZ: could not connect to ObjectManager InterfacesRemoved";
    }

    // Restore last seen devices from the store.
    for (const QString &deviceId : m_settings.lastSeenDevices()) {
        if (!m_devices.contains(deviceId)) {
            LighthouseDevice *device = nullptr;
            if (deviceId.startsWith(QLatin1String(LHB_PREFIX)) || deviceId.startsWith(QLatin1String("LHB"))) {
                device = new LighthouseV2Device(deviceId, deviceId, this);
            } else {
                device = new ViveBaseStationDevice(deviceId, deviceId, this);
            }
            m_devices.insert(deviceId, device);
        }
    }
    // Apply nicknames and Vive pair ids from the store.
    for (LighthouseDevice *device : std::as_const(m_devices)) {
        const QString nickname = m_settings.nicknameFor(device->id());
        if (!nickname.isEmpty()) {
            device->setNickname(nickname);
        }
        if (auto *vive = dynamic_cast<ViveBaseStationDevice *>(device)) {
            const int pairId = m_settings.vivePairId(device->id());
            if (pairId >= 0) {
                vive->setPairId(pairId);
            }
        }
    }

    // Devices BlueZ already knows (e.g. previously paired) show up too.
    seedKnownDevices();
    Q_EMIT devicesChanged();
}

QStringList BleManager::knownDeviceIds() const
{
    QStringList ids;
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        if (it.value()) {
            ids.append(it.key());
        }
    }
    ids.sort();
    return ids;
}

LighthouseDevice *BleManager::deviceById(const QString &id) const
{
    const auto it = m_devices.constFind(id);
    return (it != m_devices.constEnd()) ? it.value() : nullptr;
}

QList<LighthouseDevice *> BleManager::devices() const
{
    QList<LighthouseDevice *> result;
    for (auto it = m_devices.constBegin(); it != m_devices.constEnd(); ++it) {
        if (it.value()) {
            result.append(it.value());
        }
    }
    return result;
}

void BleManager::refreshAdapterState()
{
    m_adapterOn = Bluez::adapterPowered(m_adapterPath);
    Q_EMIT adapterOnChanged();
}

void BleManager::startScan()
{
    if (m_scanning || !m_adapterOn || m_adapterPath.isEmpty()) {
        return;
    }
    const bluez::Reply reply = bluez::call(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), m_adapterPath,
        IFACE_ADAPTER, QStringLiteral("StartDiscovery"));
    if (!reply.ok) {
        if (!reply.noReply) {
            qWarning() << "StartDiscovery failed:" << reply.error;
        }
        return;
    }
    m_scanning = true;
    Q_EMIT scanningChanged();
}

void BleManager::stopScan()
{
    if (!m_scanning) {
        return;
    }
    if (!m_adapterPath.isEmpty()) {
        const bluez::Reply reply = bluez::call(
            QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), m_adapterPath,
            IFACE_ADAPTER, QStringLiteral("StopDiscovery"));
        if (!reply.ok && !reply.noReply) {
            qWarning() << "StopDiscovery failed:" << reply.error;
        }
    }
    m_scanning = false;
    Q_EMIT scanningChanged();
}

void BleManager::onAdapterPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    if (iface != QLatin1String(IFACE_ADAPTER)) {
        return;
    }
    if (!changed.contains(QStringLiteral("Powered"))) {
        return;
    }
    m_adapterOn = changed.value(QStringLiteral("Powered")).toBool();
    if (!m_adapterOn && m_scanning) {
        m_scanning = false;
        Q_EMIT scanningChanged();
    }
    Q_EMIT adapterOnChanged();
}

void BleManager::onManagerInterfacesAdded(const QString &path, const QVariantMap &interfaces)
{
    const auto it = interfaces.find(QLatin1String(IFACE_DEVICE));
    if (it == interfaces.end()) {
        return;
    }
    const QVariantMap props = it.value().toMap();
    const QString address = props.value(QStringLiteral("Address")).toString();
    if (address.isEmpty()) {
        return;
    }
    m_devicePaths.insert(address.toLower(), path);
    const QString name = props.value(QStringLiteral("Name")).toString();
    const QString alias = props.value(QStringLiteral("Alias")).toString();
    const QString effective = !name.isEmpty() ? name : alias;
    if (effective.isEmpty()) {
        return;
    }
    handleDiscoveredDevice(effective, address);
}

void BleManager::onManagerInterfacesRemoved(const QString &path, const QStringList &removed)
{
    if (!removed.contains(QLatin1String(IFACE_DEVICE))) {
        return;
    }
    // Look up the address of the forgotten device before dropping it.
    const bluez::Reply address = bluez::call(
        QDBusConnection::systemBus(), QStringLiteral(ORG_BLUEZ), path,
        IFACE_PROPS, QStringLiteral("Get"),
        {QVariant(QStringLiteral(IFACE_DEVICE)), QVariant(QStringLiteral("Address"))});
    if (address.ok) {
        m_devicePaths.remove(address.value.toString().toLower());
    }
}

void BleManager::handleDiscoveredDevice(const QString &name, const QString &address)
{
    const bool isLighthouse = name.startsWith(QLatin1String(LHB_PREFIX));
    const bool isVive = name.startsWith(QLatin1String(VIVE_PREFIX));
    if (!isLighthouse && !isVive) {
        return;
    }

    LighthouseDevice *device = m_devices.value(address, nullptr);
    if (device) {
        // Refresh the display name to the latest advertisement.
        device->setNameIfNeeded(name);
        // Reconnect if a previous connection went away.
        if (!device->connected()) {
            if (BluezDevice *bluez = bluezFor(address)) {
                device->connectToDevice(bluez);
            }
        }
    } else {
        device = createDevice(name, address);
    }
    if (device) {
        m_settings.addLastSeenDevice(address);
        Q_EMIT devicesChanged();
    }
}

BluezDevice *BleManager::bluezFor(const QString &address)
{
    const QString key = address.toLower();
    BluezDevice *bluez = m_bluezDevices.value(key, nullptr);
    if (bluez) {
        return bluez;
    }
    const QString path = m_devicePaths.value(key);
    if (path.isEmpty()) {
        return nullptr;
    }
    bluez = new BluezDevice(path, this);
    m_bluezDevices.insert(key, bluez);
    return bluez;
}

LighthouseDevice *BleManager::createDevice(const QString &name, const QString &address)
{
    LighthouseDevice *device;
    if (name.startsWith(QLatin1String(LHB_PREFIX))) {
        device = new LighthouseV2Device(address, name, this);
    } else {
        device = new ViveBaseStationDevice(address, name, this);
    }

    if (BluezDevice *bluez = bluezFor(address)) {
        device->connectToDevice(bluez);
    }
    m_devices.insert(address, device);
    return device;
}

void BleManager::seedKnownDevices()
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
        const QVariantMap interfaces = it.value().toMap();
        const auto devIt = interfaces.find(QLatin1String(IFACE_DEVICE));
        if (devIt == interfaces.end()) {
            continue;
        }
        const QVariantMap props = devIt.value().toMap();
        const QString address = props.value(QStringLiteral("Address")).toString();
        if (address.isEmpty()) {
            continue;
        }
        m_devicePaths.insert(address.toLower(), path);
        const QString name = props.value(QStringLiteral("Name")).toString();
        const QString alias = props.value(QStringLiteral("Alias")).toString();
        const QString effective = !name.isEmpty() ? name : alias;
        if (effective.isEmpty()) {
            continue;
        }
        handleDiscoveredDevice(effective, address);
    }
}

bool BleManager::pairDevice(const QString &address)
{
    // Delegate to BlueZ's OS-level pairing.
    QProcess process(this);
    process.start(QStringLiteral("bluetoothctl"), {QStringLiteral("pair"), address});
    process.waitForFinished(30000);
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

void BleManager::setNickname(const QString &deviceId, const QString &nickname)
{
    m_settings.setNickname(deviceId, nickname);
    LighthouseDevice *device = deviceById(deviceId);
    if (device) {
        device->setNickname(nickname);
    }
}

void BleManager::setVivePairId(const QString &deviceId, const QString &hexId)
{
    QString cleaned;
    for (const QChar &c : hexId) {
        if (c.isDigit() || (c >= QLatin1Char('A') && c <= QLatin1Char('F'))
                || (c >= QLatin1Char('a') && c <= QLatin1Char('f'))) {
            cleaned.append(c.toUpper());
        }
    }
    // The user may enter only the first 4 digits; the hint supplies the rest.
    LighthouseDevice *device = deviceById(deviceId);
    if (auto *vive = dynamic_cast<ViveBaseStationDevice *>(device)) {
        if (cleaned.length() == 4) {
            const QString hint = vive->pairIdHint();
            if (hint.length() == 4) {
                cleaned += hint;
            }
        }
        if (cleaned.length() != 8) {
            return;
        }
        bool ok = false;
        const int id = cleaned.toUInt(&ok, 16);
        if (!ok) {
            return;
        }
        m_settings.setVivePairId(deviceId, id);
        vive->setPairId(id);
        Q_EMIT devicesChanged();
    }
}

void BleManager::clearVivePairId(const QString &deviceId)
{
    m_settings.removeVivePairId(deviceId);
    if (auto *vive = dynamic_cast<ViveBaseStationDevice *>(deviceById(deviceId))) {
        vive->setPairId(-1);
        Q_EMIT devicesChanged();
    }
}

QString BleManager::vivePairIdHint(const QString &deviceId) const
{
    if (auto *vive = dynamic_cast<const ViveBaseStationDevice *>(deviceById(deviceId))) {
        return vive->pairIdHint();
    }
    return QString();
}

QString BleManager::vivePairIdHex(const QString &deviceId) const
{
    const int id = m_settings.vivePairId(deviceId);
    return id >= 0 ? QStringLiteral("%1").arg(id, 8, 16, QLatin1Char('0')).toUpper() : QString();
}
