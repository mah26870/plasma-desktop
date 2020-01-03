/* This file is part of the KDE Project
   Copyright (c) 2007-2010 Sebastian Trueg <trueg@kde.org>
   Copyright (c) 2012-2014 Vishesh Handa <me@vhanda.in>
   Copyright (c) 2020 Benjamin Port <benjamin.port@enioka.com>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "kcm.h"
#include "fileexcludefilters.h"

#include <KPluginFactory>
#include <KPluginLoader>
#include <KAboutData>
#include <QStandardPaths>
#include <KLocalizedString>

#include <QPushButton>
#include <QDir>
#include <QProcess>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QStorageInfo>

#include <Baloo/IndexerConfig>
#include <baloo/baloosettings.h>

K_PLUGIN_FACTORY_WITH_JSON(KCMColorsFactory, "kcm_baloofile.json", registerPlugin<Baloo::ServerConfigModule>();)

using namespace Baloo;

ServerConfigModule::ServerConfigModule(QObject* parent, const QVariantList& args)
    : KQuickAddons::ManagedConfigModule(parent, args)
    , m_settings(new BalooSettings(this))
    , m_filteredFolderModel(new FilteredFolderModel(m_settings, this))
    {
    qmlRegisterType<FilteredFolderModel>();
    qmlRegisterType<BalooSettings>();

    KAboutData* about = new KAboutData(
        QStringLiteral("kcm_baloofile"), i18n("File Search"),
        QStringLiteral("0.1"), QString(), KAboutLicense::GPL,
        i18n("Copyright 2007-2010 Sebastian Trüg"));

    about->addAuthor(i18n("Sebastian Trüg"), QString(), QStringLiteral("trueg@kde.org"));
    about->addAuthor(i18n("Vishesh Handa"), QString(), QStringLiteral("vhanda@kde.org"));
    about->addAuthor(i18n("Tomaz Canabrava"), QString(), QStringLiteral("tcnaabrava@kde.org"));

    setAboutData(about);
    setButtons(Help | Apply | Default);

    connect(m_settings, &BalooSettings::excludedFoldersChanged, m_filteredFolderModel, &FilteredFolderModel::updateDirectoryList);
    connect(m_settings, &BalooSettings::foldersChanged, m_filteredFolderModel, &FilteredFolderModel::updateDirectoryList);
    m_filteredFolderModel->updateDirectoryList();
}

ServerConfigModule::~ServerConfigModule()
{
}

void ServerConfigModule::load()
{
    ManagedConfigModule::load();
    m_previouslyEnabled = m_settings->indexingEnabled();
}

void ServerConfigModule::save()
{
    ManagedConfigModule::save();

    Baloo::IndexerConfig config;
    config.setFirstRun(m_previouslyEnabled != m_settings->indexingEnabled());

    m_previouslyEnabled = m_settings->indexingEnabled();

    // Start Baloo
    if (m_settings->indexingEnabled() && !allMountPointsExcluded()) {
        const QString exe = QStandardPaths::findExecutable(QStringLiteral("baloo_file"));
        QProcess::startDetached(exe, QStringList());
    }
    else {
        QDBusMessage message = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.baloo"),
            QStringLiteral("/"),
            QStringLiteral("org.kde.baloo.main"),
            QStringLiteral("quit")
        );

        QDBusConnection::sessionBus().asyncCall(message);
    }

    // Start cleaner
    const QString exe = QStandardPaths::findExecutable(QStringLiteral("baloo_file_cleaner"));
    QProcess::startDetached(exe, QStringList());

    // Update the baloo_file's config cache
    config.refresh();
}

FilteredFolderModel *ServerConfigModule::filteredModel() const
{
    return m_filteredFolderModel;
}

bool ServerConfigModule::allMountPointsExcluded()
{
    QStringList mountPoints;
    for (const QStorageInfo &si : QStorageInfo::mountedVolumes()) {
        mountPoints.append(si.rootPath());
    }

    return m_settings->excludedFolders().toSet() == mountPoints.toSet();
}

BalooSettings *ServerConfigModule::balooSettings() const
{
    return m_settings;
}

#include "kcm.moc"
