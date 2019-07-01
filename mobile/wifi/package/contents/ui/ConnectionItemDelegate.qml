/*
 *   Copyright 2017 Martin Kacej <m.kacej@atlas.sk>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2 or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Library General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

import QtQuick 2.6
import QtQuick.Layouts 1.2
import QtQuick.Controls 2.2 as Controls
import org.kde.plasma.core 2.0 as PlasmaCore
import org.kde.plasma.networkmanagement 0.2 as PlasmaNM
import org.kde.kirigami 2.2 as Kirigami

Kirigami.SwipeListItem {

    enabled: true
    //backgroundColor: theme.backgroundColor

    property var map : []
    property bool predictableWirelessPassword: !Uuid && Type == PlasmaNM.Enums.Wireless &&
                                                    (SecurityType == PlasmaNM.Enums.StaticWep ||
                                                     SecurityType == PlasmaNM.Enums.WpaPsk ||
                                                     SecurityType == PlasmaNM.Enums.Wpa2Psk)

    RowLayout {
        anchors.leftMargin: Kirigami.Units.largeSpacing * 5
        spacing: Kirigami.Units.largeSpacing
        Kirigami.Separator {}

        Item {
            Layout.preferredWidth: Kirigami.Units.gridUnit
            Layout.preferredHeight: Kirigami.Units.gridUnit

            PlasmaCore.SvgItem {
                id: connectionSvgIcon
                elementId: mobileProxyModel.showSavedMode ? "network-wireless-connected-100" : ConnectionIcon

                svg: PlasmaCore.Svg {
                    multipleImages: true
                    imagePath: "icons/network"
                    colorGroup: PlasmaCore.ColorScope.colorGroup
                }
            }

            Controls.BusyIndicator {
                id: connectingIndicator

                anchors {
                    horizontalCenter: connectionSvgIcon.horizontalCenter
                    verticalCenter: connectionSvgIcon.verticalCenter
                }
                running: ConnectionState == PlasmaNM.Enums.Activating
                visible: running
            }
        }

        Controls.Label {
            id: connectionNameLabel

            Layout.fillWidth: true
            visible: !connectionPasswordField.visible
            elide: Text.ElideRight
            font.weight: ConnectionState == PlasmaNM.Enums.Activated ? Font.DemiBold : Font.Normal
            font.italic: ConnectionState == PlasmaNM.Enums.Activating ? true : false
            text: ItemUniqueName
            textFormat: Text.PlainText
        }
        PasswordField {
            id: connectionPasswordField
            Layout.fillWidth: true
            implicitWidth: Kirigami.Units.gridUnit *16
            securityType: SecurityType
            visible: false
            onVisibleChanged: {
                if (visible)
                    forceActiveFocus()
                connectionPasswordField.text = ""
            }
            onAccepted: {
                if (acceptableInput)
                    handler.addAndActivateConnection(DevicePath, SpecificPath, connectionPasswordField.text);
            }
        }
    }

    actions: [
        Kirigami.Action {
            iconName: "network-connect"
            visible: ConnectionState != PlasmaNM.Enums.Activated && Signal > 0
            onTriggered: changeState()
        },
        Kirigami.Action {
            iconName: "network-disconnect"
            visible: ConnectionState == PlasmaNM.Enums.Activated
            onTriggered: handler.deactivateConnection(ConnectionPath, DevicePath)
        },
        Kirigami.Action {
            iconName: "configure"
            visible: (Uuid != "")? true : false
            onTriggered: getConfigureDialog()
        },
        Kirigami.Action {
            iconName: "entry-delete"
            visible: (Uuid != "")? true : false
            onTriggered: forgetNetwork()
        }
    ]

    onClicked: {
        changeState()
    }

    function getConfigureDialog() {
        var item = applicationWindow().pageStack.push(networkDetailsViewComponent)
        item.path = ConnectionPath
        item.loadNetworkSettings()
    }

    function changeState() {
        if (Signal === 0)
            return
        if (Uuid || !predictableWirelessPassword || connectionPasswordField.visible) {
            if (ConnectionState == PlasmaNM.Enums.Deactivated) {
                if (!predictableWirelessPassword && !Uuid) {
                    handler.addAndActivateConnection(DevicePath, SpecificPath);
                } else if (connectionPasswordField.visible) {
                    if (connectionPasswordField.text != "") {
                        handler.addAndActivateConnection(DevicePath, SpecificPath, connectionPasswordFieldField.text);
                        connectionPasswordField.visible = false;
                    } else {
                        connectionPasswordField.visible = false;
                    }
                } else {
                    handler.activateConnection(ConnectionPath, DevicePath, SpecificPath);
                }
            } else{
                //show popup
            }
        } else if (predictableWirelessPassword) {
            connectionPasswordField.visible = true;
        }
    }

    function forgetNetwork() {
        deleteConnectionDialog.name = ItemUniqueName
        deleteConnectionDialog.dbusPath = ConnectionPath
        deleteConnectionDialog.open()
    }
}
