#include "lighthousedevice.h"

namespace {
// Standard GATT Device Information Service characteristic UUIDs.
constexpr char MODEL_NUMBER[] = "00002a24-0000-1000-8000-00805f9b34fb";
constexpr char SERIAL_NUMBER[] = "00002a25-0000-1000-8000-00805f9b34fb";
constexpr char FIRMWARE_REVISION[] = "00002a26-0000-1000-8000-00805f9b34fb";
constexpr char HARDWARE_REVISION[] = "00002a27-0000-1000-8000-00805f9b34fb";
constexpr char MANUFACTURER_NAME[] = "00002a29-0000-1000-8000-00805f9b34fb";
} // namespace

LighthouseDevice::LighthouseDevice(const QString &id, const QString &name, QObject *parent)
    : QObject(parent), m_id(id), m_name(name)
{
    m_pollTimer.setInterval(minUpdateInterval());
    connect(&m_pollTimer, &QTimer::timeout, this, [this] {
        if (m_connected && m_bluez) {
            pollState();
        }
    });
}

LighthouseDevice::~LighthouseDevice()
{
}

QString LighthouseDevice::displayName() const
{
    return m_nickname.isEmpty() ? m_name : m_nickname;
}

QString LighthouseDevice::nickname() const
{
    return m_nickname;
}

void LighthouseDevice::setNickname(const QString &nickname)
{
    if (m_nickname != nickname) {
        m_nickname = nickname;
        Q_EMIT displayNameChanged();
    }
}

bool LighthouseDevice::hasCharacteristic(const QString &uuid) const
{
    return m_bluez && m_bluez->hasCharacteristic(uuid);
}

void LighthouseDevice::readCharacteristic(const QString &uuid)
{
    if (m_bluez) {
        m_bluez->readCharacteristic(uuid);
    }
}

bool LighthouseDevice::writeCharacteristic(const QString &uuid, const QByteArray &value, bool withoutResponse)
{
    if (!m_bluez || !m_bluez->hasCharacteristic(uuid)) {
        return false;
    }
    m_bluez->writeCharacteristic(uuid, value, withoutResponse);
    return true;
}

QString LighthouseDevice::characteristicPathInService(const QString &serviceUuid, const QString &uuid) const
{
    return m_bluez ? m_bluez->characteristicPathInService(serviceUuid, uuid) : QString();
}

void LighthouseDevice::writeCharacteristicPath(const QString &charPath, const QByteArray &value, bool withoutResponse)
{
    if (m_bluez) {
        m_bluez->writeCharacteristicPath(charPath, value, withoutResponse);
    }
}

void LighthouseDevice::readStringCharacteristic(const QString &uuid, const QString &metaKey)
{
    if (!hasCharacteristic(uuid)) {
        return;
    }
    m_metaKeysByUuid.insert(uuid, metaKey);
    // Read is async; the value arrives through onCharacteristicRead().
    readCharacteristic(uuid);
}

void LighthouseDevice::onCharacteristicRead(const QString &uuid, const QByteArray &value)
{
    const auto it = m_metaKeysByUuid.constFind(uuid);
    if (it != m_metaKeysByUuid.constEnd()) {
        const QString text = QString::fromUtf8(value).trimmed();
        m_metadata.insert(it.value(), text);
        if (it.value() == QStringLiteral("Firmware version")) {
            m_firmwareVersion = text;
        }
        Q_EMIT metadataChanged();
    }
}

void LighthouseDevice::setPowerState(int byte)
{
    if (m_powerStateByte != byte) {
        m_powerStateByte = byte;
        m_powerState = static_cast<int>(powerStateFromByte(byte));
        Q_EMIT powerStateChanged();
    }
}

void LighthouseDevice::connectToDevice(BluezDevice *bluez)
{
    if (!bluez) {
        return;
    }
    if (m_bluez == bluez) {
        // Reconnect after a previous disconnect.
        if (!m_connected) {
            m_bluez->connectGatt();
        }
        return;
    }
    m_bluez = bluez;
    connect(m_bluez, &BluezDevice::gattConnectedChanged, this, [this](bool connected) {
        if (m_connected == connected) {
            return;
        }
        m_connected = connected;
        Q_EMIT connectedChanged();
        if (!connected) {
            m_valid = false;
            m_pollTimer.stop();
            onDisconnected();
        }
    });
    connect(m_bluez, &BluezDevice::discoveryFinished, this, [this] {
        if (!m_bluez) {
            return;
        }
        if (onServicesDiscovered()) {
            m_valid = true;
            m_pollTimer.start();
        } else {
            m_bluez->disconnectGatt();
        }
    });
    connect(m_bluez, &BluezDevice::characteristicRead, this, [this](const QString &uuid, const QByteArray &value) {
        onCharacteristicRead(uuid, value);
    });
    connect(m_bluez, &BluezDevice::nameChanged, this, [this](const QString &name) {
        setNameIfNeeded(name);
    });
    // If discovery already ran (cached GATT DB), handle it immediately.
    if (m_bluez->discoveryDone()) {
        if (onServicesDiscovered()) {
            m_valid = true;
            if (m_connected) {
                m_pollTimer.start();
            }
        }
    }
    m_bluez->connectGatt();
}

void LighthouseDevice::disconnect()
{
    m_pollTimer.stop();
    if (m_bluez) {
        m_bluez->disconnectGatt();
    }
    m_connected = false;
    m_valid = false;
    Q_EMIT connectedChanged();
}

bool LighthouseDevice::changeState(int newState)
{
    Q_UNUSED(newState);
    return false;
}

void LighthouseDevice::identify()
{
}

bool LighthouseDevice::onServicesDiscovered()
{
    // Read standard metadata characteristics (best effort).
    readStringCharacteristic(MODEL_NUMBER, QStringLiteral("Model number"));
    readStringCharacteristic(SERIAL_NUMBER, QStringLiteral("Serial number"));
    readStringCharacteristic(FIRMWARE_REVISION, QStringLiteral("Firmware version"));
    readStringCharacteristic(HARDWARE_REVISION, QStringLiteral("Hardware revision"));
    readStringCharacteristic(MANUFACTURER_NAME, QStringLiteral("Manufacturer name"));
    return true;
}

void LighthouseDevice::pollState()
{
}

void LighthouseDevice::onDisconnected()
{
}
