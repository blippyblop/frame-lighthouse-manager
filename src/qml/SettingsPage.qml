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

    Kirigami.ColumnView {
        Kirigami.Heading {
            text: "Lighthouse Power management"
            level: 1
        }

        Kirigami.Separator {}

        Kirigami.Form {
            Kirigami.FormGroup {
                title: qsTr("Power")

                Kirigami.FormEntry {
                    title: qsTr("Use STANDBY instead of SLEEP")
                    subtitle: qsTr("Only V2 lighthouses support standby.")
                    contentItem: QQC2.Switch {
                        id: standbySwitch
                        checked: settingsStore.useStandby()
                        onToggled: settingsStore.setUseStandby(checked)
                    }
                }
            }

            Kirigami.FormGroup {
                title: qsTr("Intervals")

                Kirigami.FormEntry {
                    title: qsTr("Scan duration")
                    contentItem: QQC2.SpinBox {
                        id: scanDurationBox
                        from: 10
                        to: 600
                        stepSize: 10
                        value: settingsStore.scanDuration()
                        onValueChanged: settingsStore.setScanDuration(value)
                    }
                }

                Kirigami.FormEntry {
                    title: qsTr("Update interval")
                    contentItem: QQC2.SpinBox {
                        id: updateIntervalBox
                        from: 1
                        to: 60
                        stepSize: 1
                        value: settingsStore.updateInterval()
                        onValueChanged: settingsStore.setUpdateInterval(value)
                    }
                }
            }
        }

        Kirigami.Heading {
            text: qsTr("Lighthouses with nicknames")
            level: 3
        }
        Kirigami.ColumnView {
            Layout.fillHeight: true
            ListView {
                model: nicknameListModel
                delegate: Item {
                    required property var model
                    height: Kirigami.Units.gridSize * 2
                    Kirigami.ColumnView {
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

        Kirigami.ColumnView {
            QQC2.ToolButton {
                icon.name: "edit-clear-all"
                text: qsTr("Clear all last seen devices")
                onClicked: {
                    settingsStore.clearLastSeenDevices()
                    nicknameListModel.refresh()
                }
            }
        }
        Kirigami.ColumnView {
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
