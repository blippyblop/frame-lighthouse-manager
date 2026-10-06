#include "blemanager.h"

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QLowEnergyController>
#include <QProcess>

#include "lighthousedevice.h"
#include "lighthousev2device.h"
#include "vivebasestation.h"

namespace {
constexpr char LHB_PREFIX[] = "LHB-";
constexpr char VIVE_PREFIX[] = "HTC BS";
} // namespace

BleManager::BleManager(QObject *parent)
    : QObject(parent)
{
    m_localDevice = new QBluetoothLocalDevice(this);
    refreshAdapterState();
    connect(m_localDevice, &QBluetoothLocalDevice::hostModeStateChanged,
            this, [this](QBluetoothLocalDevice::HostMode mode) {
                m_adapterOn = mode != QBluetoothLocalDevice::HostPoweredOff;
                Q_EMIT adapterOnChanged();
            });

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
    m_adapterOn = m_localDevice && m_localDevice->hostMode() != QBluetoothLocalDevice::HostPoweredOff;
    Q_EMIT adapterOnChanged();
}

void BleManager::startScan()
{
    if (m_scanning || !m_adapterOn) {
        return;
    }
    delete m_discoveryAgent;
    m_discoveryAgent = new QBluetoothDeviceDiscoveryAgent(this);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered,
            this, &BleManager::onDeviceDiscovered);
    connect(m_discoveryAgent, &QBluetoothDeviceDiscoveryAgent::finished,
            this, &BleManager::onDiscoveryFinished);
    m_discoveryAgent->start(QBluetoothDeviceDiscoveryAgent::LowEnergyMethod);
    m_scanning = true;
    Q_EMIT scanningChanged();
}

void BleManager::stopScan()
{
    if (!m_scanning) {
        return;
    }
    if (m_discoveryAgent) {
        m_discoveryAgent->stop();
    }
    m_scanning = false;
    Q_EMIT scanningChanged();
}

void BleManager::onDiscoveryFinished()
{
    if (m_scanning) {
        m_scanning = false;
        Q_EMIT scanningChanged();
    }
}

void BleManager::onDeviceDiscovered(const QBluetoothDeviceInfo &info)
{
    const QString name = info.name();
    if (name.isEmpty() || !info.isValid()) {
        return;
    }
    const bool isLighthouse = name.startsWith(QLatin1String(LHB_PREFIX));
    const bool isVive = name.startsWith(QLatin1String(VIVE_PREFIX));
    if (!isLighthouse && !isVive) {
        return;
    }

    const QString address = info.address().toString();
    LighthouseDevice *device = m_devices.value(address, nullptr);
    if (device) {
        // Refresh the display name to the latest advertisement.
        device->setNameIfNeeded(name);
    } else {
        device = createDevice(info);
    }
    if (device) {
        m_settings.addLastSeenDevice(address);
        Q_EMIT devicesChanged();
    }
}

LighthouseDevice *BleManager::createDevice(const QBluetoothDeviceInfo &info)
{
    const QString name = info.name();
    const QString address = info.address().toString();

    LighthouseDevice *device;
    if (name.startsWith(QLatin1String(LHB_PREFIX))) {
        device = new LighthouseV2Device(address, name, this);
    } else {
        device = new ViveBaseStationDevice(address, name, this);
    }

    auto *controller = QLowEnergyController::createCentral(info, this);
    if (!controller) {
        return device;
    }
    device->connectToDevice(controller);
    m_devices.insert(address, device);
    return device;
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
