/*
    SPDX-FileCopyrightText: 2024 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include <KConfig>
#include <KConfigGroup>
#include <KDesktopFile>

template<typename Config>
static KConfigGroup resolveGroup(Config &config, const QString &path)
{
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    KConfigGroup ret = config.group(parts.first());
    for (int i = 1; i < parts.size(); ++i) {
        ret = ret.group(parts[i]);
    }
    return ret;
}

static QList<KConfigGroup> selectComponents(const QList<KConfigGroup> &pool, const QStringList &patterns, int index = 0)
{
    if (pool.isEmpty()) {
        return QList<KConfigGroup>();
    }

    QList<KConfigGroup> next;
    const QRegularExpression expression(QRegularExpression::wildcardToRegularExpression(patterns[index]));
    for (const KConfigGroup &candidate : pool) {
        if (expression.match(candidate.name()).hasMatch()) {
            next.append(candidate);
        }
    }

    if (patterns.size() - 1 == index) {
        return next;
    }

    QList<KConfigGroup> children;
    for (const KConfigGroup &cg : next) {
        const QStringList groupNames = cg.groupList();
        for (const QString &groupName : groupNames) {
            children.append(cg.group(groupName));
        }
    }

    return selectComponents(children, patterns, index + 1);
}

static QList<KConfigGroup> selectComponents(KConfigBase &config, const QString &pattern)
{
    const QStringList parts = pattern.split(QLatin1Char('/'), Qt::SkipEmptyParts);

    QList<KConfigGroup> candidates;
    const QStringList groupNames = config.groupList();
    for (const QString &groupName : groupNames) {
        candidates.append(config.group(groupName));
    }

    return selectComponents(candidates, parts);
}

static bool isSpecialKey(const QString &key)
{
    return key == QStringLiteral("_k_friendly_name");
}

struct Migration {
    KConfigGroup sourceGroup;
    QString sourceAction;
    KConfigGroup targetGroup;
    QString targetAction;
    QString targetShortcut;
    QString targetDisplayName;

    void execute()
    {
        if (isSpecialKey(sourceAction)) {
            return;
        }

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
                qFatal("Invalid config file path: %s", qPrintable(configFilePath));
            }
        } else {
            configFilePath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kglobalshortcutsrc");
        }

        KConfig config(configFilePath);

        QList<KConfigGroup> sourceGroups;
        QString sourceAction;
        if (parser.isSet(sourceComponentOption)) {
            const QString sourceComponentPattern = parser.value(sourceComponentOption);
            if (sourceComponentPattern.isEmpty()) {
                qFatal("Missing source component pattern");
            }

            sourceGroups = selectComponents(config, sourceComponentPattern);
            sourceAction = parser.value(sourceActionOption);
        }
        if (sourceGroups.isEmpty()) {
            return 0; // nothing to do
        }

        KConfigGroup targetGroup;
        QString targetComponentPath;
        QString targetAction;
        QString targetShortcut;
        QString targetDisplayName;
        if (parser.isSet(targetComponentOption)) {
            targetComponentPath = parser.value(targetComponentOption);
            if (targetComponentPath.isEmpty()) {
                qFatal("Invalid target component");
            }
            targetGroup = resolveGroup(config, targetComponentPath);
            targetAction = parser.value(targetActionOption);
        } else if (parser.isSet(targetDesktopFileOption)) {
            if (sourceGroups.size() > 1) {
                qFatal("Invalid input: expected one source componet");
            }

            const QString desktopFilePath = parser.value(targetDesktopFileOption);
            if (!KDesktopFile::isDesktopFile(desktopFilePath)) {
                qFatal("%s is not a desktop file", qPrintable(desktopFilePath));
            }

            const KDesktopFile desktopFile(desktopFilePath);

            targetGroup = resolveGroup(config, QLatin1String("/services/") + QFileInfo(desktopFile.fileName()).fileName());
            if (parser.isSet(targetDesktopFileActionOption)) {
                const QString actionName = parser.value(targetDesktopFileActionOption);
                const KConfigGroup actionGroup = desktopFile.actionGroup(actionName);
                if (!actionGroup.exists()) {
                    qFatal("Specified action %s does not exist", qPrintable(actionName));
                }
                targetAction = actionName;
                targetShortcut = actionGroup.readEntry<QString>(QLatin1String("X-KDE-Shortcuts"), QString());
                targetDisplayName = actionGroup.readEntry<QString>(QLatin1String("Name"), QString());
            } else if (const KConfigGroup group = desktopFile.desktopGroup(); group.hasKey(QLatin1String("X-KDE-Shortcuts"))) {
                targetAction = QLatin1String("_launch");
                targetShortcut = group.readEntry<QString>(QLatin1String("X-KDE-Shortcuts"), QString());
                targetDisplayName = desktopFile.readName();
            }
        }

        for (KConfigGroup sourceGroup : std::as_const(sourceGroups)) {
            KConfigGroup effectiveTargetGroup;
            if (targetComponentPath.endsWith(QLatin1Char('/'))) {
                effectiveTargetGroup = targetGroup.group(sourceGroup.name());
            } else {
                effectiveTargetGroup = targetGroup;
            }

            if (!sourceAction.isEmpty()) {
                (Migration{
                     .sourceGroup = sourceGroup,
                     .sourceAction = sourceAction,
                     .targetGroup = effectiveTargetGroup,
                     .targetAction = targetAction,
                     .targetShortcut = targetShortcut,
                     .targetDisplayName = targetDisplayName,
                 })
                    .execute();
            } else {
                const QStringList entries = sourceGroup.keyList();
                for (const QString &entry : entries) {
                    (Migration{
                         .sourceGroup = sourceGroup,
                         .sourceAction = entry,
                         .targetGroup = effectiveTargetGroup,
                         .targetAction = targetAction,
                         .targetShortcut = targetShortcut,
                         .targetDisplayName = targetDisplayName,
                     })
                        .execute();
                }
            }
        }

        config.sync();
    } else {
        parser.showHelp();
        return -1;
    }

    return 0;
}
