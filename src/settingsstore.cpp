#include "settingsstore.h"

#include <KConfigCore/KConfig>
#include <KConfigCore/KConfigGroup>

namespace {
constexpr char NICKNAMES_GROUP[] = "Nicknames";
constexpr char GROUPS_GROUP[] = "Groups";
constexpr char LAST_SEEN_GROUP[] = "LastSeenDevices";
constexpr char VIVE_IDS_GROUP[] = "ViveBaseStationIds";
constexpr char SETTINGS_GROUP[] = "Settings";
} // namespace

SettingsStore::SettingsStore(QObject *parent)
    : QObject(parent)
{
    m_config = new KConfig(QStringLiteral("lighthouse-pm-kde"), KConfig::SimpleConfig);
    reload();
}

void SettingsStore::reload()
{
    // KConfig loads lazily and caches per instance; nothing to re-read.
}

void SettingsStore::write()
{
    m_config->sync();
}

QString SettingsStore::nicknameFor(const QString &deviceId) const
{
    KConfigGroup group(m_config, NICKNAMES_GROUP);
    return group.readEntry(deviceId, QString());
}

void SettingsStore::setNickname(const QString &deviceId, const QString &nickname)
{
    KConfigGroup group(m_config, NICKNAMES_GROUP);
    if (nickname.isEmpty()) {
        group.deleteEntry(deviceId);
    } else {
        group.writeEntry(deviceId, nickname);
    }
    write();
    Q_EMIT changed();
}

void SettingsStore::removeNickname(const QString &deviceId)
{
    setNickname(deviceId, QString());
}

QMap<QString, QString> SettingsStore::nicknames() const
{
    QMap<QString, QString> result;
    KConfigGroup group(m_config, NICKNAMES_GROUP);
    for (const QString &key : group.keyList()) {
        result.insert(key, group.readEntry(key, QString()));
    }
    return result;
}

QStringList SettingsStore::groupNames() const
{
    KConfigGroup group(m_config, GROUPS_GROUP);
    return group.keyList();
}

QStringList SettingsStore::groupMembers(const QString &groupName) const
{
    KConfigGroup group(m_config, GROUPS_GROUP);
    const QString raw = group.readEntry(groupName, QString());
    return raw.isEmpty() ? QStringList{} : raw.split(',', Qt::SkipEmptyParts);
}

void SettingsStore::createGroup(const QString &groupName)
{
    if (groupName.isEmpty()) {
        return;
    }
    KConfigGroup group(m_config, GROUPS_GROUP);
    if (!group.keyList().contains(groupName)) {
        group.writeEntry(groupName, QString());
    }
    write();
    Q_EMIT changed();
}

void SettingsStore::renameGroup(const QString &oldName, const QString &newName)
{
    if (newName.isEmpty() || oldName == newName) {
        return;
    }
    KConfigGroup group(m_config, GROUPS_GROUP);
    if (!group.keyList().contains(oldName)) {
        return;
    }
    const QString members = group.readEntry(oldName, QString());
    group.deleteEntry(oldName);
    group.writeEntry(newName, members);
    write();
    Q_EMIT changed();
}

void SettingsStore::removeGroup(const QString &groupName)
{
    KConfigGroup group(m_config, GROUPS_GROUP);
    group.deleteEntry(groupName);
    write();
    Q_EMIT changed();
}

void SettingsStore::setGroupMembers(const QString &groupName, const QStringList &deviceIds)
{
    KConfigGroup group(m_config, GROUPS_GROUP);
    if (!group.keyList().contains(groupName)) {
        group.writeEntry(groupName, deviceIds.join(','));
    } else {
        group.writeEntry(groupName, deviceIds.join(','));
    }
    write();
    Q_EMIT changed();
}

QString SettingsStore::groupForDevice(const QString &deviceId) const
{
    for (const QString &name : groupNames()) {
        if (groupMembers(name).contains(deviceId)) {
            return name;
        }
    }
    return QString();
}

QStringList SettingsStore::lastSeenDevices() const
{
    KConfigGroup group(m_config, LAST_SEEN_GROUP);
    return group.keyList();
}

void SettingsStore::addLastSeenDevice(const QString &deviceId)
{
    KConfigGroup group(m_config, LAST_SEEN_GROUP);
    if (!group.keyList().contains(deviceId)) {
        group.writeEntry(deviceId, QStringLiteral("1"));
    }
    write();
}

void SettingsStore::clearLastSeenDevices()
{
    KConfigGroup group(m_config, LAST_SEEN_GROUP);
    for (const QString &key : group.keyList()) {
        group.deleteEntry(key);
    }
    write();
    Q_EMIT changed();
}

int SettingsStore::vivePairId(const QString &deviceId) const
{
    KConfigGroup group(m_config, VIVE_IDS_GROUP);
    if (!group.keyList().contains(deviceId)) {
        return -1;
    }
    bool ok = false;
    const int id = group.readEntry(deviceId, QStringLiteral("0")).toUInt(&ok, 16);
    return ok ? id : -1;
}

void SettingsStore::setVivePairId(const QString &deviceId, int id)
{
    KConfigGroup group(m_config, VIVE_IDS_GROUP);
    if (id < 0) {
        group.deleteEntry(deviceId);
    } else {
        group.writeEntry(deviceId, QStringLiteral("0x%1").arg(id, 8, 16, QLatin1Char('0')));
    }
    write();
    Q_EMIT changed();
}

void SettingsStore::removeVivePairId(const QString &deviceId)
{
    setVivePairId(deviceId, -1);
}

void SettingsStore::clearAllVivePairIds()
{
    KConfigGroup group(m_config, VIVE_IDS_GROUP);
    for (const QString &key : group.keyList()) {
        group.deleteEntry(key);
    }
    write();
    Q_EMIT changed();
}

bool SettingsStore::useStandby() const
{
    KConfigGroup group(m_config, SETTINGS_GROUP);
    return group.readEntry(QStringLiteral("useStandby"), false);
}

void SettingsStore::setUseStandby(bool value)
{
    KConfigGroup group(m_config, SETTINGS_GROUP);
    group.writeEntry(QStringLiteral("useStandby"), value);
    write();
    Q_EMIT changed();
}

int SettingsStore::scanDuration() const
{
    KConfigGroup group(m_config, SETTINGS_GROUP);
    return group.readEntry(QStringLiteral("scanDuration"), 60);
}

void SettingsStore::setScanDuration(int seconds)
{
    KConfigGroup group(m_config, SETTINGS_GROUP);
    group.writeEntry(QStringLiteral("scanDuration"), seconds);
    write();
}

int SettingsStore::updateInterval() const
{
    KConfigGroup group(m_config, SETTINGS_GROUP);
    return group.readEntry(QStringLiteral("updateInterval"), 1);
}

void SettingsStore::setUpdateInterval(int seconds)
{
    KConfigGroup group(m_config, SETTINGS_GROUP);
    group.writeEntry(QStringLiteral("updateInterval"), seconds);
    write();
}
