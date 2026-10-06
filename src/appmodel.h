#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QPointer>
#include <QStringList>
#include <QSet>

class LighthouseDevice;
class BleManager;
class SettingsStore;

// Provides the device list to QML: group headers followed by grouped
// devices, then ungrouped devices. Also handles selection mode.
class AppModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(bool selecting READ selecting NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedGroup READ selectedGroup NOTIFY selectionChanged)

public:
    enum RowType { GroupRow = 0, DeviceRow = 1 };
    Q_ENUM(RowType)

    explicit AppModel(BleManager *bleManager, SettingsStore *settings, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool selecting() const { return m_selecting; }
    QString selectedGroup() const { return m_selectedGroup; }

    Q_INVOKABLE void toggleSelectDevice(const QString &deviceId);
    Q_INVOKABLE void toggleSelectGroup(const QString &groupName);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE bool isSelected(const QString &deviceId) const;
    Q_INVOKABLE bool isGroupSelected(const QString &groupName) const;

    // Group management
    Q_INVOKABLE QStringList groups() const;
    Q_INVOKABLE void createGroup(const QString &name);
    Q_INVOKABLE void renameGroup(const QString &oldName, const QString &newName);
    Q_INVOKABLE void deleteGroup(const QString &groupName);
    Q_INVOKABLE void assignSelectedToGroup(const QString &groupName);
    Q_INVOKABLE void removeSelectedFromGroup();
    Q_INVOKABLE QStringList selectedDeviceIds() const;
    Q_INVOKABLE QObject *deviceById(const QString &deviceId) const;

    void refresh();

Q_SIGNALS:
    void selectionChanged();

private:
    struct Row {
        int type = DeviceRow;
        QString group;
        QString deviceId;
    };
    void rebuildRows();
    QString displayNameFor(const QString &deviceId) const;
    LighthouseDevice *device(const QString &deviceId) const;

    BleManager *m_ble = nullptr;
    SettingsStore *m_settings = nullptr;
    QList<Row> m_rows;
    bool m_selecting = false;
    QSet<QString> m_selectedDevices;
    QString m_selectedGroup;
};
