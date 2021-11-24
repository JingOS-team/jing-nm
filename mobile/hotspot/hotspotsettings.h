/*
 *   Copyright 2020 Tobias Fella <fella@posteo.de>
 *   Copyright 2021 Liu Bangguo <liubangguo@jingos.com>
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


#ifndef HOTSPOTSETTINGS_H
#define HOTSPOTSETTINGS_H


#include <KQuickAddons/ConfigModule>

class HotspotSettings : public KQuickAddons::ConfigModule
{
    Q_OBJECT
public:
    HotspotSettings(QObject *parent, const QVariantList &args);
    virtual ~HotspotSettings() override;
    Q_INVOKABLE void setDeviceName(const QString name);
    Q_INVOKABLE QString getDeviceName();
    Q_INVOKABLE QString getRadomPassword();
};

#endif // HOTSPOTSETTINGS_H
