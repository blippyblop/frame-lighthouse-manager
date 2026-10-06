#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

class KConfig;
class KConfigGroup;

// KDE-native persistence via KConfig: nicknames, groups, last seen
// devices, Vive base station ids and settings.
class SettingsStore : public QObject
{
    Q_OBJECT

public:
    explicit SettingsStore(QObject *parent = nullptr);

    // Nicknames
    QString nicknameFor(const QString &deviceId) const;
    void setNickname(const QString &deviceId, const QString &nickname);
    void removeNickname(const QString &deviceId);
    QMap<QString, QString> nicknames() const;

    // Groups: name -> list of device ids
    QStringList groupNames() const;
    QStringList groupMembers(const QString &groupName) const;
    void createGroup(const QString &groupName);
    void renameGroup(const QString &oldName, const QString &newName);
    void removeGroup(const QString &groupName);
    void setGroupMembers(const QString &groupName, const QStringList &deviceIds);
    QString groupForDevice(const QString &deviceId) const;

    // Last seen devices
    QStringList lastSeenDevices() const;
    void addLastSeenDevice(const QString &deviceId);
    void clearLastSeenDevices();

    // Vive base station pair ids
    int vivePairId(const QString &deviceId) const; // -1 if not stored
    void setVivePairId(const QString &deviceId, int id);
    void removeVivePairId(const QString &deviceId);
    void clearAllVivePairIds();

    // Settings
    bool useStandby() const;
    void setUseStandby(bool value);
    int scanDuration() const;
    void setScanDuration(int seconds);
    int updateInterval() const; // seconds
    void setUpdateInterval(int seconds);

    void reload();

Q_SIGNALS:
    void changed();

private:
    void write();
    KConfig *m_config = nullptr;
};
