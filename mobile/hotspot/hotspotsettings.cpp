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

#include "hotspotsettings.h"

#include <KPluginFactory>
#include <KLocalizedString>
#include <KAboutData>
#include <KConfigGroup>
#include <KSharedConfig>
#include <KLocalizedString>

K_PLUGIN_CLASS_WITH_JSON(HotspotSettings, "metadata.json")

HotspotSettings::HotspotSettings(QObject* parent, const QVariantList& args) : KQuickAddons::ConfigModule(parent, args)
{
    KLocalizedString::setApplicationDomain("kcm_mobile_hotspot");

    KAboutData* about = new KAboutData("kcm_mobile_hotspot", i18n("Hotspot"),
                                       "0.1", QString(), KAboutLicense::LGPL);
    about->addAuthor(i18n("Tobias Fella"), QString(), "fella@posteo.de");
    setAboutData(about);
}

HotspotSettings::~HotspotSettings()
{
}

void HotspotSettings::setDeviceName(const QString name)
{
    auto kdeglobals = KSharedConfig::openConfig("kdeglobals");
    KConfigGroup cfg(kdeglobals, "SystemInfo");
    cfg.writeEntry("deviceName", name);
    kdeglobals->sync();
}

QString HotspotSettings::getDeviceName()
{
    auto kdeglobals = KSharedConfig::openConfig("kdeglobals");
    KConfigGroup cfg(kdeglobals, "SystemInfo");
    // return cfg.readEntry("deviceName", QString());
    return cfg.readEntry("deviceName", "JingOS");
}

QString HotspotSettings::getRadomPassword()
{
    QString str = QUuid::createUuid().toString();
    str.remove(QRegularExpression("{|}|-"));
    return str.mid(0,8);
}

#include "hotspotsettings.moc"
