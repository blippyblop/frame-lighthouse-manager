import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: "Lighthouse Power Management"
    visible: true
    minimumWidth: Kirigami.Units.gridSize * 40
    minimumHeight: Kirigami.Units.gridSize * 30

    property bool showSelectionToolbar: appModel.selecting
    property string metadataTarget: ""

    function openMetadata(deviceId) {
        metadataTarget = deviceId
        pageStack.push(metadataPageComponent)
    }

    globalDrawer: Kirigami.GlobalDrawer {
        actions: [
            Kirigami.Action {
                icon.name: "security-lock"
                text: qsTr("Pair device")
                onTriggered: pageStack.push(pairPageComponent)
            },
            Kirigami.Action {
                icon.name: "documenthelp"
                text: qsTr("Help")
                onTriggered: pageStack.push(helpPageComponent)
            },
            Kirigami.Action {
                icon.name: "preferences-system"
                text: qsTr("Settings")
                onTriggered: pageStack.push(settingsPageComponent)
            }
        ]
    }

    pageStack.initialPage: mainPageComponent

    Component {
        id: mainPageComponent

        Kirigami.Page {
            id: mainPage

            // Rendered into the global toolbar by the ApplicationWindow.
            actions: [
                Kirigami.Action {
                    icon.name: bleManager.scanning ? "process-stop" : "find"
                    text: bleManager.scanning ? qsTr("Stop scanning") : qsTr("Scan for lighthouses")
                    onTriggered: bleManager.scanning ? bleManager.stopScan() : bleManager.startScan()
                },
                Kirigami.Action {
                    icon.name: "bluetooth"
                    text: qsTr("Pair a new device")
                    onTriggered: pageStack.push(pairPageComponent)
                }
            ]

            Column {
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Status hints
                Kirigami.InlineMessage {
                    visible: bleManager.scanning
                    text: qsTr("Scanning for lighthouses...")
                }
                Kirigami.InlineMessage {
                    visible: !bleManager.adapterOn
                    text: qsTr("Bluetooth is off. Turn it on to talk to the lighthouses.")
                }

                Column {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        id: listView
                        model: appModel
                        spacing: Kirigami.Units.smallSpacing

                        delegate: appModel.rowType === appModel.GroupRow
                            ? groupDelegate : deviceDelegate

                        Component {
                            id: groupDelegate
                            Item {
                                id: groupRow
                                required property string groupName
                                required property int memberCount

                                height: Kirigami.Units.gridSize * 2
                                Kirigami.Heading {
                                    anchors.fill: parent
                                    anchors.margins: Kirigami.Units.smallSpacing
                                    text: "%1 (%2)".arg(groupName, memberCount)
                                    level: 2
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: appModel.toggleSelectGroup(groupName)
                                }
                            }
                        }

                        Component {
                            id: deviceDelegate
                            Item {
                            id: deviceRow
                            required property string deviceId
                            required property string displayName
                            required property string powerStateText
                            required property int powerState
                            required property bool connected
                            required property bool selected
                            required property string group
                            required property var deviceObject

                            height: Kirigami.Units.gridSize * 3

                            function togglePower() {
                                if (!connected) {
                                    bootingHint.text = qsTr("Device is offline. Scan for it first.")
                                    bootingHint.visible = true
                                    return
                                }
                                if (powerState === 1) {
                                    deviceObject.changeState(settingsStore.useStandby() ? 4 : 0)
                                } else if (powerState === 0 || powerState === 4) {
                                    deviceObject.changeState(1)
                                } else if (powerState === 3) {
                                    bootingHint.text = qsTr("Lighthouse is already booting!")
                                    bootingHint.visible = true
                                } else {
                                    stateDialog.deviceId = deviceId
                                    stateDialog.open()
                                }
                            }

                            Kirigami.Card {
                                anchors.fill: parent
                                anchors.margins: Kirigami.Units.smallSpacing / 2
                                background: Rectangle {
                                    color: selected
                                        ? Kirigami.Theme.highlightColor
                                        : Kirigami.Theme.palette.base
                                }
                                Column {
                                    RowLayout {
                                        Layout.fillWidth: true
                                        ColumnLayout {
                                            id: textColumn
                                            Layout.fillWidth: true
                                            Kirigami.Heading {
                                                text: displayName
                                                level: 2
                                            }
                                            Text {
                                                text: "%1 · %2".arg(powerStateText, deviceId)
                                                color: Kirigami.Theme.disabledTextColor
                                                font.pixelSize: Kirigami.Units.fontPixelSize - 2
                                            }
                                            MouseArea {
                                                anchors.fill: parent
                                                onClicked: root.openMetadata(deviceId)
                                                onPressAndHold: appModel.toggleSelectDevice(deviceId)
                                            }
                                        }
                                        QQC2.ToolButton {
                                            Layout.preferredWidth: Kirigami.Units.gridSize * 2
                                            Layout.preferredHeight: Kirigami.Units.gridSize * 2
                                            text: ""
                                            icon.name: "system-power"
                                            icon.color: powerState === 1
                                                ? Kirigami.Theme.positiveColor
                                                : powerState === 3
                                                ? Kirigami.Theme.negativeColor
                                                : powerState === 2
                                                ? Kirigami.Theme.disabledTextColor
                                                : Kirigami.Theme.textColor
                                            QQC2.ToolTip.text: qsTr("Toggle power state")
                                            onClicked: togglePower()
                                        }
                                    }
                                }
                            }
                        }
                        }
                    }
                }
            }

            Kirigami.InlineMessage {
                id: bootingHint
                text: ""
            }

            Kirigami.PromptDialog {
                id: nicknameDialog
                title: qsTr("Nickname")
                subtitle: qsTr("Enter a nickname for the selected device. Blank removes it.")
                standardButtons: Kirigami.Dialog.Cancel
                customFooterActions: [
                    Kirigami.Action {
                        text: qsTr("Set nickname")
                        onTriggered: {
                            var ids = appModel.selectedDeviceIds()
                            if (ids.length > 0) {
                                bleManager.setNickname(ids[0], nicknameField.text)
                            }
                            nicknameDialog.close()
                        }
                    }
                ]
                QQC2.TextField {
                    id: nicknameField
                    text: ""
                }
            }

            Kirigami.PromptDialog {
                id: groupDialog
                title: appModel.selectedGroup !== "" ? qsTr("Rename group") : qsTr("Add to group")
                subtitle: appModel.selectedGroup !== ""
                    ? qsTr("New name for the group.")
                    : qsTr("Add the selected devices to a group. Blank = no group.")
                standardButtons: Kirigami.Dialog.Cancel
                customFooterActions: [
                    Kirigami.Action {
                        text: qsTr("Apply")
                        onTriggered: {
                            var name = groupField.text.trim()
                            if (appModel.selectedGroup !== "") {
                                appModel.renameGroup(appModel.selectedGroup, name)
                            } else {
                                appModel.assignSelectedToGroup(name)
                            }
                            groupDialog.close()
                        }
                    }
                ]
                QQC2.TextField {
                    id: groupField
                    text: appModel.selectedGroup
                }
            }

            Kirigami.PromptDialog {
                id: stateDialog
                title: qsTr("Current state unknown")
                subtitle: qsTr("Pick the new state for this device.")
                property string deviceId: ""
                standardButtons: Kirigami.Dialog.Cancel
                customFooterActions: [
                    Kirigami.Action {
                        text: qsTr("On")
                        onTriggered: { appModel.deviceById(stateDialog.deviceId).changeState(1); stateDialog.close() }
                    },
                    Kirigami.Action {
                        text: qsTr("Standby")
                        onTriggered: { appModel.deviceById(stateDialog.deviceId).changeState(4); stateDialog.close() }
                    },
                    Kirigami.Action {
                        text: qsTr("Sleep")
                        onTriggered: { appModel.deviceById(stateDialog.deviceId).changeState(0); stateDialog.close() }
                    }
                ]
            }

            Kirigami.PromptDialog {
                id: deleteGroupDialog
                title: qsTr("Delete group")
                subtitle: qsTr("Remove the group '%1'? Devices stay in the list.")
                    .arg(appModel.selectedGroup)
                standardButtons: Kirigami.Dialog.Cancel
                customFooterActions: [
                    Kirigami.Action {
                        text: qsTr("Delete")
                        onTriggered: {
                            appModel.deleteGroup(appModel.selectedGroup)
                            deleteGroupDialog.close()
                        }
                    }
                ]
            }
        }
    }

    Component {
        id: metadataPageComponent
        MetadataPage {
            deviceId: metadataTarget
        }
    }
    Component {
        id: pairPageComponent
        PairPage {}
    }
    Component {
        id: settingsPageComponent
        SettingsPage {}
    }
    Component {
        id: helpPageComponent
        HelpPage {}
    }
}
