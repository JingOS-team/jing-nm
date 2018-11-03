/*
    Copyright 2013-2016 Jan Grulich <jgrulich@redhat.com>
    Copyright 2013, 2014 Lukas Tinkl <ltinkl@redhat.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "connectioneditorbase.h"

#include "debug.h"
#include "settings/bondwidget.h"
#include "settings/bridgewidget.h"
#include "settings/btwidget.h"
#include "settings/cdmawidget.h"
#include "settings/connectionwidget.h"
#include "settings/gsmwidget.h"
#include "settings/infinibandwidget.h"
#include "settings/ipv4widget.h"
#include "settings/ipv6widget.h"
#include "settings/pppwidget.h"
#include "settings/pppoewidget.h"
#include "settings/teamwidget.h"
#include "settings/vlanwidget.h"
#include "settings/wificonnectionwidget.h"
#include "settings/wifisecurity.h"
#include "settings/wiredconnectionwidget.h"
#include "settings/wiredsecurity.h"
#include "vpnuiplugin.h"

#include <NetworkManagerQt/ActiveConnection>
#include <NetworkManagerQt/AdslSetting>
#include <NetworkManagerQt/Connection>
#include <NetworkManagerQt/CdmaSetting>
#include <NetworkManagerQt/GenericTypes>
#include <NetworkManagerQt/GsmSetting>
#include <NetworkManagerQt/PppoeSetting>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/VpnSetting>
#include <NetworkManagerQt/Utils>
#include <NetworkManagerQt/WirelessSetting>
#include <NetworkManagerQt/WirelessDevice>

#include <KIconLoader>
#include <KLocalizedString>
#include <KNotification>
#include <KServiceTypeTrader>
#include <KUser>

ConnectionEditorBase::ConnectionEditorBase(const NetworkManager::ConnectionSettings::Ptr &connection,
                                           QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
    , m_initialized(false)
    , m_valid(false)
    , m_pendingReplies(0)
    , m_connection(connection)
{
}

ConnectionEditorBase::ConnectionEditorBase(QWidget *parent, Qt::WindowFlags f)
    : QWidget(parent, f)
{
}

ConnectionEditorBase::~ConnectionEditorBase()
{
    m_connection.clear();
}

void ConnectionEditorBase::setConnection(const NetworkManager::ConnectionSettings::Ptr &connection)
{
    // Set connection settings
    m_connection.clear();
    m_connection = connection;
    m_initialized = false;

    // Reset UI setting widgets
    delete m_connectionWidget;
    m_connectionWidget = nullptr;
    qDeleteAll(m_settingWidgets);
    m_settingWidgets.clear();

    initialize();
}

NMVariantMapMap ConnectionEditorBase::setting() const
{
    NMVariantMapMap settings = m_connectionWidget->setting();

    Q_FOREACH (SettingWidget *widget, m_settingWidgets) {
        const QString type = widget->type();
        if (type != NetworkManager::Setting::typeAsString(NetworkManager::Setting::Security8021x) &&
                type != NetworkManager::Setting::typeAsString(NetworkManager::Setting::WirelessSecurity)) {
            settings.insert(type, widget->setting());
        }

        // add 802.1x security if needed
        QVariantMap security8021x;
        if (type == NetworkManager::Setting::typeAsString(NetworkManager::Setting::WirelessSecurity)) {
            WifiSecurity *wifiSecurity = static_cast<WifiSecurity *>(widget);
            if (wifiSecurity->enabled()) {
                settings.insert(type, wifiSecurity->setting());
            }
            if (wifiSecurity->enabled8021x()) {
                security8021x = static_cast<WifiSecurity *>(widget)->setting8021x();
                settings.insert(NetworkManager::Setting::typeAsString(NetworkManager::Setting::Security8021x), security8021x);
            }
        } else if (type == NetworkManager::Setting::typeAsString(NetworkManager::Setting::Security8021x)) {
            WiredSecurity *wiredSecurity = static_cast<WiredSecurity *>(widget);
            if (wiredSecurity->enabled8021x()) {
                security8021x = static_cast<WiredSecurity *>(widget)->setting();
                settings.insert(NetworkManager::Setting::typeAsString(NetworkManager::Setting::Security8021x), security8021x);
            }
        }
    }

    // Set properties which are not returned from setting widgets
    NetworkManager::ConnectionSettings::Ptr connectionSettings = NetworkManager::ConnectionSettings::Ptr(new NetworkManager::ConnectionSettings(m_connection->connectionType()));

    connectionSettings->fromMap(settings);
    connectionSettings->setId(connectionName());
    connectionSettings->setUuid(m_connection->uuid());

    if (connectionSettings->connectionType() == NetworkManager::ConnectionSettings::Wireless) {
        NetworkManager::WirelessSecuritySetting::Ptr securitySetting = connectionSettings->setting(NetworkManager::Setting::WirelessSecurity).staticCast<NetworkManager::WirelessSecuritySetting>();
        NetworkManager::WirelessSetting::Ptr wirelessSetting = connectionSettings->setting(NetworkManager::Setting::Wireless).staticCast<NetworkManager::WirelessSetting>();

        if (securitySetting && wirelessSetting) {
            if (securitySetting->keyMgmt() != NetworkManager::WirelessSecuritySetting::WirelessSecuritySetting::Unknown) {
                wirelessSetting->setSecurity("802-11-wireless-security");
            }
        }
    }

    return connectionSettings->toMap();
}

bool ConnectionEditorBase::isInitialized() const
{
    return m_initialized;
}

bool ConnectionEditorBase::isValid() const
{
    return m_valid;
}

void ConnectionEditorBase::addConnectionWidget(ConnectionWidget *widget, const QString &text)
{
    m_connectionWidget = widget;

    connect(widget, &ConnectionWidget::settingChanged, this, &ConnectionEditorBase::settingChanged);

    addWidget(widget, text);
}

void ConnectionEditorBase::addSettingWidget(SettingWidget *widget, const QString &text)
{
    m_settingWidgets << widget;

    connect(widget, &SettingWidget::settingChanged, this, &ConnectionEditorBase::settingChanged);

    addWidget(widget, text);
}

void ConnectionEditorBase::initialize()
{
    const bool emptyConnection = m_connection->id().isEmpty();
    const NetworkManager::ConnectionSettings::ConnectionType type = m_connection->connectionType();

    if (emptyConnection) {
        m_connection->addToPermissions(KUser().loginName(), QString());
    }

    // General configuration common to all connection types
    ConnectionWidget *connectionWidget = new ConnectionWidget(m_connection);
    addConnectionWidget(connectionWidget, i18nc("General", "General configuration"));

    // Add the rest of widgets
    QString serviceType;
    if (type == NetworkManager::ConnectionSettings::Wired) {
        WiredConnectionWidget *wiredWidget = new WiredConnectionWidget(m_connection->setting(NetworkManager::Setting::Wired), this);
        addSettingWidget(wiredWidget, i18n("Wired"));
        WiredSecurity *wiredSecurity = new WiredSecurity(m_connection->setting(NetworkManager::Setting::Security8021x).staticCast<NetworkManager::Security8021xSetting>(), this);
        addSettingWidget(wiredSecurity, i18n("802.1x Security"));
    } else if (type == NetworkManager::ConnectionSettings::Wireless) {
        WifiConnectionWidget *wifiWidget = new WifiConnectionWidget(m_connection->setting(NetworkManager::Setting::Wireless), this);
        addSettingWidget(wifiWidget, i18n("Wi-Fi"));
        WifiSecurity *wifiSecurity = new WifiSecurity(m_connection->setting(NetworkManager::Setting::WirelessSecurity),
                m_connection->setting(NetworkManager::Setting::Security8021x).staticCast<NetworkManager::Security8021xSetting>(),
                this);
        addSettingWidget(wifiSecurity, i18n("Wi-Fi Security"));
        connect(wifiWidget, QOverload<const QString &>::of(&WifiConnectionWidget::ssidChanged), wifiSecurity, &WifiSecurity::onSsidChanged);
    } else if (type == NetworkManager::ConnectionSettings::Pppoe) { // DSL
        PppoeWidget *pppoeWidget = new PppoeWidget(m_connection->setting(NetworkManager::Setting::Pppoe), this);
        addSettingWidget(pppoeWidget, i18n("DSL"));
        WiredConnectionWidget *wiredWidget = new WiredConnectionWidget(m_connection->setting(NetworkManager::Setting::Wired), this);
        addSettingWidget(wiredWidget, i18n("Wired"));
    } else if (type == NetworkManager::ConnectionSettings::Gsm) { // GSM
        GsmWidget *gsmWidget = new GsmWidget(m_connection->setting(NetworkManager::Setting::Gsm), this);
        addSettingWidget(gsmWidget, i18n("Mobile Broadband (%1)", m_connection->typeAsString(m_connection->connectionType())));
    } else if (type == NetworkManager::ConnectionSettings::Cdma) { // CDMA
        CdmaWidget *cdmaWidget = new CdmaWidget(m_connection->setting(NetworkManager::Setting::Cdma), this);
        addSettingWidget(cdmaWidget, i18n("Mobile Broadband (%1)", m_connection->typeAsString(m_connection->connectionType())));
    } else if (type == NetworkManager::ConnectionSettings::Bluetooth) {  // Bluetooth
        BtWidget *btWidget = new BtWidget(m_connection->setting(NetworkManager::Setting::Bluetooth), this);
        addSettingWidget(btWidget, i18n("Bluetooth"));
        NetworkManager::BluetoothSetting::Ptr btSetting = m_connection->setting(NetworkManager::Setting::Bluetooth).staticCast<NetworkManager::BluetoothSetting>();
        if (btSetting->profileType() == NetworkManager::BluetoothSetting::Dun) {
            GsmWidget *gsmWidget = new GsmWidget(m_connection->setting(NetworkManager::Setting::Gsm), this);
            addSettingWidget(gsmWidget, i18n("GSM"));
            PPPWidget *pppWidget = new PPPWidget(m_connection->setting(NetworkManager::Setting::Ppp), this);
            addSettingWidget(pppWidget, i18n("PPP"));
        }
    } else if (type == NetworkManager::ConnectionSettings::Infiniband) { // Infiniband
        InfinibandWidget *infinibandWidget = new InfinibandWidget(m_connection->setting(NetworkManager::Setting::Infiniband), this);
        addSettingWidget(infinibandWidget, i18n("Infiniband"));
    } else if (type == NetworkManager::ConnectionSettings::Bond) { // Bond
        BondWidget *bondWidget = new BondWidget(m_connection->uuid(), m_connection->setting(NetworkManager::Setting::Bond), this);
        addSettingWidget(bondWidget, i18n("Bond"));
    } else if (type == NetworkManager::ConnectionSettings::Bridge) { // Bridge
        BridgeWidget *bridgeWidget = new BridgeWidget(m_connection->uuid(), m_connection->setting(NetworkManager::Setting::Bridge), this);
        addSettingWidget(bridgeWidget, i18n("Bridge"));
    } else if (type == NetworkManager::ConnectionSettings::Vlan) { // Vlan
        VlanWidget *vlanWidget = new VlanWidget(m_connection->setting(NetworkManager::Setting::Vlan), this);
        addSettingWidget(vlanWidget, i18n("Vlan"));
    } else if (type == NetworkManager::ConnectionSettings::Team) { // Team
        TeamWidget *teamWidget = new TeamWidget(m_connection->uuid(), m_connection->setting(NetworkManager::Setting::Team), this);
        addSettingWidget(teamWidget, i18n("Team"));
    } else if (type == NetworkManager::ConnectionSettings::Vpn) { // VPN
        QString error;
        VpnUiPlugin *vpnPlugin = nullptr;
        NetworkManager::VpnSetting::Ptr vpnSetting =
            m_connection->setting(NetworkManager::Setting::Vpn).staticCast<NetworkManager::VpnSetting>();
        if (!vpnSetting) {
            qCWarning(PLASMA_NM) << "Missing VPN setting!";
        } else {
            serviceType = vpnSetting->serviceType();
            vpnPlugin = KServiceTypeTrader::createInstanceFromQuery<VpnUiPlugin>(QString::fromLatin1("PlasmaNetworkManagement/VpnUiPlugin"),
                        QString::fromLatin1("[X-NetworkManager-Services]=='%1'").arg(serviceType),
                        this, QVariantList(), &error);
            if (vpnPlugin && error.isEmpty()) {
                const QString shortName = serviceType.section('.', -1);
                SettingWidget *vpnWidget = vpnPlugin->widget(vpnSetting, this);
                addSettingWidget(vpnWidget, i18n("VPN (%1)", shortName));
            } else {
                qCWarning(PLASMA_NM) << error << ", serviceType == " << serviceType;
            }
        }
    }

    // PPP widget
    if (type == NetworkManager::ConnectionSettings::Pppoe || type == NetworkManager::ConnectionSettings::Cdma || type == NetworkManager::ConnectionSettings::Gsm) {
        PPPWidget *pppWidget = new PPPWidget(m_connection->setting(NetworkManager::Setting::Ppp), this);
        addSettingWidget(pppWidget, i18n("PPP"));
    }

    // IPv4 widget
    if (!m_connection->isSlave()) {
        IPv4Widget *ipv4Widget = new IPv4Widget(m_connection->setting(NetworkManager::Setting::Ipv4), this);
        addSettingWidget(ipv4Widget, i18n("IPv4"));
    }

    // IPv6 widget
    if ((type == NetworkManager::ConnectionSettings::Wired
            || type == NetworkManager::ConnectionSettings::Wireless
            || type == NetworkManager::ConnectionSettings::Infiniband
#if NM_CHECK_VERSION(0, 9, 10)
            || type == NetworkManager::ConnectionSettings::Team
#endif

#if NM_CHECK_VERSION(1, 0, 0)
            || type == NetworkManager::ConnectionSettings::Cdma
            || type == NetworkManager::ConnectionSettings::Gsm
#endif
            || type == NetworkManager::ConnectionSettings::Bond
            || type == NetworkManager::ConnectionSettings::Bridge
            || type == NetworkManager::ConnectionSettings::Vlan
            || (type == NetworkManager::ConnectionSettings::Vpn && serviceType == QLatin1String("org.freedesktop.NetworkManager.openvpn"))) && !m_connection->isSlave()) {
        IPv6Widget *ipv6Widget = new IPv6Widget(m_connection->setting(NetworkManager::Setting::Ipv6), this);
        addSettingWidget(ipv6Widget, i18n("IPv6"));
    }

    // Re-check validation
    bool valid = true;
    Q_FOREACH (SettingWidget *widget, m_settingWidgets) {
        valid = valid && widget->isValid();
        connect(widget, &SettingWidget::validChanged, this, &ConnectionEditorBase::validChanged);
    }

    m_valid = valid;
    Q_EMIT validityChanged(valid);

    KAcceleratorManager::manage(this);

    // If the connection is not empty (not new) we want to load its secrets
    if (!emptyConnection) {
        NetworkManager::Connection::Ptr connection = NetworkManager::findConnectionByUuid(m_connection->uuid());
        if (connection) {
            QStringList requiredSecrets;
            QString settingName;
            QVariantMap setting;
            QDBusPendingReply<NMVariantMapMap> reply;

            if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Adsl) {
                NetworkManager::AdslSetting::Ptr adslSetting = connection->settings()->setting(NetworkManager::Setting::Adsl).staticCast<NetworkManager::AdslSetting>();
                if (adslSetting && !adslSetting->needSecrets().isEmpty()) {
                    requiredSecrets = adslSetting->needSecrets();
                    setting = adslSetting->toMap();
                    settingName = QLatin1String("adsl");
                }
            } else if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Bluetooth) {
                NetworkManager::GsmSetting::Ptr gsmSetting = connection->settings()->setting(NetworkManager::Setting::Gsm).staticCast<NetworkManager::GsmSetting>();
                if (gsmSetting && !gsmSetting->needSecrets().isEmpty()) {
                    requiredSecrets = gsmSetting->needSecrets();
                    setting = gsmSetting->toMap();
                    settingName = QLatin1String("gsm");
                }
            } else if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Cdma) {
                NetworkManager::CdmaSetting::Ptr cdmaSetting = connection->settings()->setting(NetworkManager::Setting::Cdma).staticCast<NetworkManager::CdmaSetting>();
                if (cdmaSetting && !cdmaSetting->needSecrets().isEmpty()) {
                    requiredSecrets = cdmaSetting->needSecrets();
                    setting = cdmaSetting->toMap();
                    settingName = QLatin1String("cdma");
                }
            } else if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Gsm) {
                NetworkManager::GsmSetting::Ptr gsmSetting = connection->settings()->setting(NetworkManager::Setting::Gsm).staticCast<NetworkManager::GsmSetting>();
                if (gsmSetting && !gsmSetting->needSecrets().isEmpty()) {
                    requiredSecrets = gsmSetting->needSecrets();
                    setting = gsmSetting->toMap();
                    settingName = QLatin1String("gsm");
                }
            } else if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Pppoe) {
                NetworkManager::PppoeSetting::Ptr pppoeSetting = connection->settings()->setting(NetworkManager::Setting::Pppoe).staticCast<NetworkManager::PppoeSetting>();
                if (pppoeSetting && !pppoeSetting->needSecrets().isEmpty()) {
                    requiredSecrets = pppoeSetting->needSecrets();
                    setting = pppoeSetting->toMap();
                    settingName = QLatin1String("pppoe");
                }
            } else if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Wired) {
                NetworkManager::Security8021xSetting::Ptr securitySetting = connection->settings()->setting(NetworkManager::Setting::Security8021x).staticCast<NetworkManager::Security8021xSetting>();
                if (securitySetting && !securitySetting->needSecrets().isEmpty()) {
                    requiredSecrets = securitySetting->needSecrets();
                    setting = securitySetting->toMap();
                    settingName = QLatin1String("802-1x");
                }
            } else if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Wireless) {
                NetworkManager::WirelessSecuritySetting::Ptr wifiSecuritySetting = connection->settings()->setting(NetworkManager::Setting::WirelessSecurity).staticCast<NetworkManager::WirelessSecuritySetting>();
                if (wifiSecuritySetting &&
                        (wifiSecuritySetting->keyMgmt() == NetworkManager::WirelessSecuritySetting::WpaEap ||
                         (wifiSecuritySetting->keyMgmt() == NetworkManager::WirelessSecuritySetting::WirelessSecuritySetting::Ieee8021x &&
                          wifiSecuritySetting->authAlg() != NetworkManager::WirelessSecuritySetting::Leap))) {
                    NetworkManager::Security8021xSetting::Ptr securitySetting = connection->settings()->setting(NetworkManager::Setting::Security8021x).staticCast<NetworkManager::Security8021xSetting>();
                    if (securitySetting && !securitySetting->needSecrets().isEmpty()) {
                        requiredSecrets = securitySetting->needSecrets();
                        setting = securitySetting->toMap();
                        settingName = QLatin1String("802-1x");

                        if (requiredSecrets.contains(NM_SETTING_802_1X_PASSWORD_RAW)) {
                            requiredSecrets.removeAll(NM_SETTING_802_1X_PASSWORD_RAW);
                        }
                    }
                } else {
                    if (!wifiSecuritySetting->needSecrets().isEmpty()) {
                        requiredSecrets = wifiSecuritySetting->needSecrets();
                        setting = wifiSecuritySetting->toMap();
                        settingName = QLatin1String("802-11-wireless-security");
                    }
                }
            } else if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Vpn) {
                settingName = QLatin1String("vpn");
            }

            if (!requiredSecrets.isEmpty() || m_connection->connectionType() == NetworkManager::ConnectionSettings::Vpn) {
                bool requestSecrets = false;
                if (m_connection->connectionType() == NetworkManager::ConnectionSettings::Vpn) {
                    requestSecrets = true;
                } else {
                    Q_FOREACH (const QString &secret, requiredSecrets) {
                        if (setting.contains(secret + QLatin1String("-flags"))) {
                            NetworkManager::Setting::SecretFlagType secretFlag = (NetworkManager::Setting::SecretFlagType)setting.value(secret + QLatin1String("-flags")).toInt();
                            if (secretFlag == NetworkManager::Setting::None || secretFlag == NetworkManager::Setting::AgentOwned) {
                                requestSecrets = true;
                            }
                        } else {
                            requestSecrets = true;
                        }
                    }
                }

                if (requestSecrets) {
                    m_pendingReplies++;
                    reply = connection->secrets(settingName);
                    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
                    watcher->setProperty("connection", connection->name());
                    watcher->setProperty("settingName", settingName);
                    connect(watcher, &QDBusPendingCallWatcher::finished, this, &ConnectionEditorBase::replyFinished);
                    m_valid = false;
                    Q_EMIT validityChanged(false);
                    return;
                }
            }
        }
    }

    // We should be now fully initialized as we don't wait for secrets
    if (m_pendingReplies == 0) {
        m_initialized = true;
    }
}

void ConnectionEditorBase::replyFinished(QDBusPendingCallWatcher *watcher)
{
    QDBusPendingReply<NMVariantMapMap> reply = *watcher;
    const QString settingName = watcher->property("settingName").toString();
    if (reply.isValid()) {
        NMVariantMapMap secrets = reply.argumentAt<0>();
        Q_FOREACH (const QString &key, secrets.keys()) {
            if (key == settingName) {
                NetworkManager::Setting::Ptr setting = m_connection->setting(NetworkManager::Setting::typeFromString(key));
                if (setting) {
                    setting->secretsFromMap(secrets.value(key));
                    Q_FOREACH (SettingWidget *widget, m_settingWidgets) {
                        const QString type = widget->type();
                        if (type == settingName ||
                                (settingName == NetworkManager::Setting::typeAsString(NetworkManager::Setting::Security8021x) &&
                                 type == NetworkManager::Setting::typeAsString(NetworkManager::Setting::WirelessSecurity))) {
                            widget->loadSecrets(setting);
                        }
                    }
                }
            }
        }
    } else {
        KNotification *notification = new KNotification("FailedToGetSecrets", KNotification::CloseOnTimeout);
        notification->setComponentName("networkmanagement");
        notification->setTitle(i18n("Failed to get secrets for %1", watcher->property("connection").toString()));
        notification->setText(reply.error().message());
        notification->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(KIconLoader::SizeHuge));
        notification->sendEvent();
    }

    watcher->deleteLater();
    validChanged(true);

    // We should be now fully with secrets
    m_pendingReplies--;
    m_initialized = true;
}

void ConnectionEditorBase::validChanged(bool valid)
{
    if (!valid) {
        m_valid = false;
        Q_EMIT validityChanged(false);
        return;
    } else {
        Q_FOREACH (SettingWidget *widget, m_settingWidgets) {
            if (!widget->isValid()) {
                m_valid = false;
                Q_EMIT validityChanged(false);
                return;
            }
        }
    }

    m_valid = true;
    Q_EMIT validityChanged(true);
}
