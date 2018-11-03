/*
    Copyright (c) 2013 Lukas Tinkl <ltinkl@redhat.com>

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

#ifndef PLASMA_NM_IPV6_WIDGET_H
#define PLASMA_NM_IPV6_WIDGET_H

#include <QWidget>
#include <NetworkManagerQt/Ipv6Setting>

#include "settingwidget.h"
#include "ipv6routeswidget.h"

namespace Ui
{
class IPv6Widget;
}

class Q_DECL_EXPORT IPv6Widget : public SettingWidget
{
Q_OBJECT
public:
    enum MethodIndex { Automatic = 0, AutomaticOnlyIP, AutomaticOnlyDHCP, LinkLocal, Manual, Disabled };

    explicit IPv6Widget(const NetworkManager::Setting::Ptr &setting = NetworkManager::Setting::Ptr(), QWidget* parent = nullptr, Qt::WindowFlags f = {});
    ~IPv6Widget() override;

    void loadConfig(const NetworkManager::Setting::Ptr &setting) override;

    QVariantMap setting() const override;

    bool isValid() const override;

private Q_SLOTS:
    void slotModeComboChanged(int index);
    void slotRoutesDialog();

    void slotDnsServers();
    void slotDnsDomains();

    void slotAddIPAddress();
    void slotRemoveIPAddress();

    void selectionChanged(const QItemSelection & selected);
    void tableViewItemChanged(QStandardItem * item);

private:
    Ui::IPv6Widget * m_ui;
    NetworkManager::Ipv6Setting m_tmpIpv6Setting;

    class Private;
    Private * const d;
};

#endif // PLASMA_NM_IPV4_WIDGET_H
