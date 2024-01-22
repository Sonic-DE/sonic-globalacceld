/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>

#include <KConfig>
#include <KConfigGroup>
#include <KDesktopFile>

template<typename Config>
static KConfigGroup resolveGroup(Config &config, const QStringList &path)
{
    KConfigGroup ret = config.group(path.first());
    for (int i = 1; i < path.size(); ++i) {
        ret = ret.group(path[i]);
    }
    return ret;
}

static bool isSpecialKey(const QString &key)
{
    return key == QStringLiteral("_k_friendly_name");
}

struct Migration {
    QStringList sourceComponent;
    QString sourceAction;
    QStringList targetComponent;
    QString targetAction;
    QString targetShortcut;
    QString targetDisplayName;

    void execute(KConfigBase &config) const
    {
        if (isSpecialKey(sourceAction)) {
            return;
        }

        KConfigGroup sourceGroup = resolveGroup(config, sourceComponent);
        if (!sourceGroup.hasKey(sourceAction)) {
            return;
        }

        const QStringList actionList = sourceGroup.readEntry<QStringList>(sourceAction, QStringList());
        const QString shortcut = actionList.size() > 0 ? actionList.at(0) : QString();

        QString defaultShortcut = targetShortcut;
        if (defaultShortcut.isEmpty()) {
            if (actionList.size() > 1) {
                defaultShortcut = actionList.at(1);
            }
        }

        QString displayText = targetDisplayName;
        if (displayText.isEmpty()) {
            if (actionList.size() > 2) {
                displayText = actionList.at(2);
            }
        }

        if (shortcut != defaultShortcut) {
            QString entry = targetAction;
            if (entry.isEmpty()) {
                entry = sourceAction;
            }
            KConfigGroup targetGroup = resolveGroup(config, targetComponent);
            targetGroup.writeEntry(entry, QStringList{shortcut, defaultShortcut, displayText});
        }

        sourceGroup.deleteEntry(sourceAction);
        if (sourceGroup.entryMap().isEmpty() || (sourceGroup.entryMap().size() == 1 && isSpecialKey(sourceGroup.entryMap().firstKey()))) {
            sourceGroup.deleteGroup();
        }
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setOptionsAfterPositionalArgumentsMode(QCommandLineParser::ParseAsOptions);
    parser.addPositionalArgument(QStringLiteral("subcommand"), QStringLiteral("move"));

    parser.parse(app.arguments());
    QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        parser.showHelp();
        return -1;
    }

    const QString subcommand = args.first();
    if (subcommand == QLatin1String("move")) {
        QCommandLineOption configOption(QStringLiteral("config"), QStringLiteral("Path to kglobalshortcutsrc"), QStringLiteral("config"));

        QCommandLineOption sourceComponentOption(QStringLiteral("source-component"), QStringLiteral("Source component"), QStringLiteral("source-component"));
        QCommandLineOption sourceActionOption(QStringLiteral("source-action"), QStringLiteral("Source action"), QStringLiteral("source-action"));
        QCommandLineOption targetComponentOption(QStringLiteral("target-component"), QStringLiteral("Target component"), QStringLiteral("target-component"));
        QCommandLineOption targetActionOption(QStringLiteral("target-action"), QStringLiteral("Target action"), QStringLiteral("target-action"));

        QCommandLineOption targetDesktopFileOption(QStringLiteral("target-desktop-file"),
                                                   QStringLiteral("Target desktop file"),
                                                   QStringLiteral("target-desktop-file"));
        QCommandLineOption targetDesktopFileActionOption(QStringLiteral("target-desktop-file-action"),
                                                         QStringLiteral("Target desktop file action"),
                                                         QStringLiteral("target-desktop-file-action"));

        parser.addOption(configOption);
        parser.addOption(sourceComponentOption);
        parser.addOption(sourceActionOption);
        parser.addOption(targetComponentOption);
        parser.addOption(targetActionOption);
        parser.addOption(targetDesktopFileOption);
        parser.addOption(targetDesktopFileActionOption);
        parser.addHelpOption();

        parser.process(app.arguments());
        if (!parser.isSet(sourceComponentOption)) {
            qWarning() << "Missing --source-component";
            parser.showHelp();
            return -1;
        }

        if (!(parser.isSet(targetComponentOption) || parser.isSet(targetDesktopFileOption))) {
            qWarning() << "Missing --target-component or --target-desktop-file";
            parser.showHelp();
            return -1;
        }

        QString configFilePath;
        if (parser.isSet(configOption)) {
            configFilePath = parser.value(configOption);
            if (configFilePath.isEmpty()) {
                qWarning() << "Invalid config file path:" << configFilePath;
                return -1;
            }
        } else {
            configFilePath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kglobalshortcutsrc");
        }

        if (!QFileInfo::exists(configFilePath)) {
            return 0; // nothing to do
        }

        KConfig config(configFilePath);
        QList<Migration> migrations;

        QStringList targetComponent;
        QString targetAction;
        QString targetShortcut;
        QString targetDisplayName;
        if (parser.isSet(targetComponentOption)) {
            targetComponent = parser.value(targetComponentOption).split(QLatin1Char('/'));
            if (targetComponent.isEmpty()) {
                qWarning() << "Missing target component";
                return -1;
            }
            targetAction = parser.value(targetActionOption);
        } else if (parser.isSet(targetDesktopFileOption)) {
            const QString desktopFilePath = parser.value(targetDesktopFileOption);
            if (!KDesktopFile::isDesktopFile(desktopFilePath)) {
                qWarning() << "Invalid desktop file name:" << desktopFilePath;
                return -1;
            }

            const KDesktopFile desktopFile(desktopFilePath);

            targetComponent = {QLatin1String("services"), QFileInfo(desktopFile.fileName()).fileName()};
            if (parser.isSet(targetDesktopFileActionOption)) {
                const QString actionName = parser.value(targetDesktopFileActionOption);
                const KConfigGroup actionGroup = desktopFile.actionGroup(actionName);
                if (!actionGroup.exists()) {
                    qWarning() << "Specified action" << actionName << "does not exist";
                    return -1;
                }
                targetAction = actionName;
                targetShortcut = actionGroup.readEntry<QString>(QLatin1String("X-KDE-Shortcuts"), QString());
                targetDisplayName = actionGroup.readEntry<QString>(QLatin1String("Name"), QString());
            } else if (const KConfigGroup group = desktopFile.desktopGroup(); group.hasKey(QLatin1String("X-KDE-Shortcuts"))) {
                targetAction = QLatin1String("_launch");
                targetShortcut = group.readEntry<QString>(QLatin1String("X-KDE-Shortcuts"), QString());
                targetDisplayName = desktopFile.readName();
            }
        } else {
            Q_UNREACHABLE();
        }

        QStringList sourceComponent;
        QString sourceAction;
        if (parser.isSet(sourceComponentOption)) {
            sourceComponent = parser.value(sourceComponentOption).split(QLatin1Char('/'));
            if (sourceComponent.isEmpty()) {
                qWarning() << "Missing source component";
                return -1;
            }
            sourceAction = parser.value(sourceActionOption);

            const KConfigGroup sourceGroup = resolveGroup(config, sourceComponent);
            if (!sourceGroup.exists()) {
                return 0; // nothing to do
            }

            if (!sourceAction.isEmpty()) {
                if (!sourceGroup.hasKey(sourceAction)) {
                    return 0;
                }
                migrations.append(Migration{
                    .sourceComponent = sourceComponent,
                    .sourceAction = sourceAction,
                    .targetComponent = targetComponent,
                    .targetAction = targetAction,
                    .targetShortcut = targetShortcut,
                    .targetDisplayName = targetDisplayName,
                });
            } else {
                const QStringList entries = sourceGroup.keyList();
                for (const QString &entry : entries) {
                    migrations.append(Migration{
                        .sourceComponent = sourceComponent,
                        .sourceAction = entry,
                        .targetComponent = targetComponent,
                        .targetAction = targetAction,
                        .targetShortcut = targetShortcut,
                        .targetDisplayName = targetDisplayName,
                    });
                }
            }
        }

        for (const Migration &migration : std::as_const(migrations)) {
            migration.execute(config);
        }

        config.sync();
    } else {
        parser.showHelp();
        return -1;
    }

    return 0;
}
