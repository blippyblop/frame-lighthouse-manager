#include "appmodel.h"

#include <algorithm>

#include "blemanager.h"
#include "lighthousedevice.h"
#include "settingsstore.h"

AppModel::AppModel(BleManager *bleManager, SettingsStore *settings, QObject *parent)
    : QAbstractListModel(parent), m_ble(bleManager), m_settings(settings)
{
    if (m_ble) {
        connect(m_ble, &BleManager::devicesChanged, this, &AppModel::refresh);
    }
    if (m_settings) {
        connect(m_settings, &SettingsStore::changed, this, &AppModel::refresh);
    }
    rebuildRows();
}

int AppModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_rows.size();
}

QHash<int, QByteArray> AppModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "display";
    roles[0x100] = "rowType";
    roles[0x101] = "groupName";
    roles[0x102] = "deviceId";
    roles[0x103] = "displayName";
    roles[0x104] = "powerStateText";
    roles[0x105] = "powerState";
    roles[0x106] = "macText";
    roles[0x107] = "connected";
    roles[0x108] = "selected";
    roles[0x109] = "selecting";
    roles[0x110] = "deviceType";
    roles[0x111] = "memberCount";
    roles[0x112] = "deviceObject";
    return roles;
}

QVariant AppModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return QVariant();
    }
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return row.type == GroupRow ? row.group : displayNameFor(row.deviceId);
    case 0x100: return row.type;
    case 0x101: return row.group;
    case 0x102: return row.deviceId;
    case 0x103: return displayNameFor(row.deviceId);
    case 0x104: {
        LighthouseDevice *d = device(row.deviceId);
        return d ? d->powerStateText() : QStringLiteral("Offline");
    }
    case 0x105: {
        LighthouseDevice *d = device(row.deviceId);
        return d ? d->powerState() : 2;
    }
    case 0x106: return row.deviceId;
    case 0x107: {
        LighthouseDevice *d = device(row.deviceId);
        return d ? d->connected() : false;
    }
    case 0x108: return m_selectedDevices.contains(row.deviceId);
    case 0x109: return m_selecting;
    case 0x110: {
        LighthouseDevice *d = device(row.deviceId);
        return d ? d->deviceType() : QString();
    }
    case 0x111: return m_settings ? m_settings->groupMembers(row.group).size() : 0;
    case 0x112: return QVariant::fromValue(device(row.deviceId));
    default: return QVariant();
    }
}

void AppModel::toggleSelectDevice(const QString &deviceId)
{
    if (m_selectedDevices.contains(deviceId)) {
        m_selectedDevices.remove(deviceId);
        if (m_selectedDevices.isEmpty() && m_selectedGroup.isEmpty()) {
            m_selecting = false;
        }
    } else {
        m_selecting = true;
        m_selectedDevices.insert(deviceId);
    }
    Q_EMIT selectionChanged();
    beginResetModel();
    rebuildRows();
    endResetModel();
}

void AppModel::toggleSelectGroup(const QString &groupName)
{
    m_selecting = true;
    if (m_selectedGroup == groupName) {
        m_selectedGroup.clear();
        if (m_selectedDevices.isEmpty()) {
            m_selecting = false;
        }
    } else {
        m_selectedGroup = groupName;
        m_selectedDevices.clear();
    }
    Q_EMIT selectionChanged();
    beginResetModel();
    rebuildRows();
    endResetModel();
}

void AppModel::clearSelection()
{
    m_selecting = false;
    m_selectedDevices.clear();
    m_selectedGroup.clear();
    Q_EMIT selectionChanged();
    beginResetModel();
    rebuildRows();
    endResetModel();
}

bool AppModel::isSelected(const QString &deviceId) const
{
    return m_selectedDevices.contains(deviceId);
}

bool AppModel::isGroupSelected(const QString &groupName) const
{
    return m_selectedGroup == groupName;
}

QStringList AppModel::groups() const
{
    QStringList result = m_settings ? m_settings->groupNames() : QStringList{};
    std::sort(result.begin(), result.end());
    return result;
}

void AppModel::createGroup(const QString &name)
{
    if (m_settings && !name.isEmpty()) {
        m_settings->createGroup(name);
    }
}

void AppModel::renameGroup(const QString &oldName, const QString &newName)
{
    if (m_settings) {
        m_settings->renameGroup(oldName, newName);
        if (m_selectedGroup == oldName) {
            m_selectedGroup = newName;
        }
        Q_EMIT selectionChanged();
    }
}

void AppModel::deleteGroup(const QString &groupName)
{
    if (m_settings) {
        m_settings->removeGroup(groupName);
    }
    if (m_selectedGroup == groupName) {
        m_selectedGroup.clear();
        if (m_selectedDevices.isEmpty()) {
            m_selecting = false;
            Q_EMIT selectionChanged();
        }
    }
}

void AppModel::assignSelectedToGroup(const QString &groupName)
{
    if (!m_settings) {
        return;
    }
    if (groupName.isEmpty()) {
        removeSelectedFromGroup();
        return;
    }
    m_settings->createGroup(groupName);
    QStringList members = m_settings->groupMembers(groupName);
    const QSet<QString> selected = m_selectedDevices;
    for (const QString &id : selected) {
        if (!members.contains(id)) {
            members.append(id);
        }
    }
    // Devices belong to exactly one group: remove them from any other group.
    for (const QString &name : m_settings->groupNames()) {
        if (name == groupName) {
            continue;
        }
        QStringList other = m_settings->groupMembers(name);
        for (const QString &id : selected) {
            other.removeAll(id);
        }
        m_settings->setGroupMembers(name, other);
    }
    m_settings->setGroupMembers(groupName, members);
    clearSelection();
}

void AppModel::removeSelectedFromGroup()
{
    if (!m_settings) {
        return;
    }
    for (const QString &name : m_settings->groupNames()) {
        QStringList members = m_settings->groupMembers(name);
        for (const QString &id : m_selectedDevices) {
            members.removeAll(id);
        }
        m_settings->setGroupMembers(name, members);
    }
    if (!m_selectedGroup.isEmpty()) {
        m_selectedGroup.clear();
    }
}

QStringList AppModel::selectedDeviceIds() const
{
    QStringList result;
    for (const QString &id : std::as_const(m_selectedDevices)) {
        result.append(id);
    }
    std::sort(result.begin(), result.end());
    return result;
}

void AppModel::refresh()
{
    beginResetModel();
    rebuildRows();
    endResetModel();
}

QString AppModel::displayNameFor(const QString &deviceId) const
{
    LighthouseDevice *d = device(deviceId);
    if (d) {
        return d->displayName();
    }
    return deviceId;
}

LighthouseDevice *AppModel::device(const QString &deviceId) const
{
    return m_ble ? m_ble->deviceById(deviceId) : nullptr;
}

QObject *AppModel::deviceById(const QString &deviceId) const
{
    return device(deviceId);
}

void AppModel::rebuildRows()
{
    m_rows.clear();
    if (!m_ble || !m_settings) {
        return;
    }

    const QStringList groupNames = groups();
    QSet<QString> grouped;

    for (const QString &group : groupNames) {
        m_rows.append(Row{GroupRow, group, QString()});
        const QStringList members = m_settings->groupMembers(group);
        for (const QString &id : members) {
            if (device(id)) {
                m_rows.append(Row{DeviceRow, group, id});
                grouped.insert(id);
            }
        }
    }

    QStringList ungrouped;
    for (const QString &id : m_ble->knownDeviceIds()) {
        if (!grouped.contains(id)) {
            ungrouped.append(id);
        }
    }
    for (const QString &id : ungrouped) {
        m_rows.append(Row{DeviceRow, QString(), id});
    }
}
