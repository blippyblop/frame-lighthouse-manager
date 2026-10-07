import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page

    title: qsTr("Settings")

    actions: [
        Kirigami.Action {
            icon.name: "arrow-back"
            text: qsTr("Back")
            onTriggered: pageStack.pop()
        }
    ]

    Column {
        spacing: Kirigami.Units.smallSpacing
        Layout.fillWidth: true
        Layout.fillHeight: true

        Kirigami.Heading {
            text: "Lighthouse Power management"
            level: 1
        }

        Kirigami.Separator {}

        Kirigami.Heading {
            text: qsTr("Power")
            level: 3
        }
        Column {
            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                QQC2.Label {
                    text: qsTr("Use STANDBY instead of SLEEP")
                    Layout.preferredWidth: 260
                    horizontalAlignment: Text.AlignRight
                }
                QQC2.Switch {
                    id: standbySwitch
                    checked: settingsStore.useStandby()
                    onToggled: settingsStore.setUseStandby(checked)
                }
            }
            QQC2.Label {
                text: qsTr("Only V2 lighthouses support standby.")
                color: Kirigami.Theme.disabledTextColor
            }
        }

        Kirigami.Heading {
            text: qsTr("Intervals")
            level: 3
        }
        RowLayout {
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label {
                text: qsTr("Scan duration")
                Layout.preferredWidth: 260
                horizontalAlignment: Text.AlignRight
            }
            QQC2.SpinBox {
                id: scanDurationBox
                from: 10
                to: 600
                stepSize: 10
                value: settingsStore.scanDuration()
                onValueChanged: settingsStore.setScanDuration(value)
            }
        }
        RowLayout {
            spacing: Kirigami.Units.smallSpacing
            QQC2.Label {
                text: qsTr("Update interval")
                Layout.preferredWidth: 260
                horizontalAlignment: Text.AlignRight
            }
            QQC2.SpinBox {
                id: updateIntervalBox
                from: 1
                to: 60
                stepSize: 1
                value: settingsStore.updateInterval()
                onValueChanged: settingsStore.setUpdateInterval(value)
            }
        }

        Kirigami.Heading {
            text: qsTr("Lighthouses with nicknames")
            level: 3
        }
        Column {
            Layout.fillHeight: true
            Layout.fillWidth: true
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: nicknameListModel
                delegate: Item {
                    required property var model
                    height: Kirigami.Units.gridSize * 2
                    Column {
                        RowLayout {
                            Layout.fillWidth: true
                            Kirigami.Heading {
                                text: model.nickname
                                level: 2
                            }
                            Text {
                                text: model.deviceId
                                color: Kirigami.Theme.disabledTextColor
                            }
                            QQC2.ToolButton {
                                icon.name: "edit-clear"
                                text: qsTr("Remove nickname")
                                onClicked: {
                                    bleManager.setNickname(model.deviceId, "")
                                    nicknameListModel.refresh()
                                }
                            }
                        }
                    }
                }
            }
        }

        Kirigami.Separator {}

        Column {
            spacing: Kirigami.Units.smallSpacing
            QQC2.ToolButton {
                icon.name: "edit-clear-all"
                text: qsTr("Clear all last seen devices")
                onClicked: {
                    settingsStore.clearLastSeenDevices()
                    nicknameListModel.refresh()
                }
            }
            QQC2.ToolButton {
                icon.name: "edit-clear-all"
                text: qsTr("Clear all Vive base station ids")
                onClicked: settingsStore.clearAllVivePairIds()
            }
        }
    }

    ListModel {
        id: nicknameListModel

        function refresh() {
            clear()
            var ids = bleManager.knownDeviceIds()
            for (var i = 0; i < ids.length; i++) {
                var dev = bleManager.deviceById(ids[i])
                if (dev && dev.nickname.length > 0) {
                    append({ "deviceId": ids[i], "nickname": dev.nickname })
                }
            }
        }
    }

    Connections {
        target: bleManager
        function onDevicesChanged() {
            nicknameListModel.refresh()
        }
    }
    Component.onCompleted: nicknameListModel.refresh()
}
