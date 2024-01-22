/*
    SPDX-FileCopyrightText: 2023 Nicolas Fella <nicolas.fella@gmx.de>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QProcess>
#include <QStandardPaths>
#include <QTest>

#include <KConfig>
#include <KConfigGroup>

class MigrateConfigTest : public QObject
{
    Q_OBJECT

public:
    MigrateConfigTest()
    {
        QStandardPaths::setTestModeEnabled(true);
    }

private Q_SLOTS:
    void init()
    {
        QDir configDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
        configDir.mkpath(QStringLiteral("."));
        configDir.remove(QStringLiteral("kglobalshortcutsrc"));
    }

    void moveGroup_data()
    {
        QTest::addColumn<QString>("initialConfig");
        QTest::addColumn<QString>("resultConfig");
        QTest::addColumn<QString>("sourceComponent");
        QTest::addColumn<QString>("targetComponent");

        QTest::addRow("normal") << QStringLiteral("move-group") << QStringLiteral("move-group.expected") << QStringLiteral("/org.kde.foo.desktop")
                                << QStringLiteral("/services/org.kde.foo.desktop");
        QTest::addRow("reset") << QStringLiteral("move-group-reset") << QStringLiteral("move-group-reset.expected") << QStringLiteral("/org.kde.foo.desktop")
                               << QStringLiteral("/services/org.kde.foo.desktop");
        QTest::addRow("regex") << QStringLiteral("move-group-regex") << QStringLiteral("move-group-regex.expected") << QStringLiteral("/*.desktop")
                               << QStringLiteral("/services/");
    }

    void moveGroup()
    {
        QFETCH(QString, initialConfig);
        QFETCH(QString, resultConfig);
        QFETCH(QString, sourceComponent);
        QFETCH(QString, targetComponent);

        const QString configFileName = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kglobalshortcutsrc");
        QFile::copy(QFINDTESTDATA(initialConfig), configFileName);

        run({
            QStringLiteral("move"),
            QStringLiteral("--config=") + configFileName,
            QStringLiteral("--source-component=") + sourceComponent,
            QStringLiteral("--target-component=") + targetComponent,
        });

        // Compare actual with expected config
        KConfig actual(QStringLiteral("kglobalshortcutsrc"));
        KConfig expected(QFINDTESTDATA(resultConfig));

        compareGroupList(actual, expected);
        compareGroupList(actual.group(QStringLiteral("services")), expected.group(QStringLiteral("services")));
    }

    void moveAction_data()
    {
        QTest::addColumn<QString>("initialConfig");
        QTest::addColumn<QString>("resultConfig");
        QTest::addColumn<QString>("sourceComponent");
        QTest::addColumn<QString>("sourceAction");
        QTest::addColumn<QString>("targetComponent");
        QTest::addColumn<QString>("targetAction");

        QTest::addRow("normal") << QStringLiteral("move-action") << QStringLiteral("move-action.expected") << QStringLiteral("/org.kde.foo.desktop")
                                << QStringLiteral("Toggle") << QStringLiteral("/org.kde.bar.desktop") << QString();
        QTest::addRow("new-component") << QStringLiteral("move-action-new-component") << QStringLiteral("move-action-new-component.expected")
                                       << QStringLiteral("/org.kde.foo.desktop") << QStringLiteral("Toggle") << QStringLiteral("/org.kde.bar.desktop")
                                       << QString();
    }

    void moveAction()
    {
        QFETCH(QString, initialConfig);
        QFETCH(QString, resultConfig);
        QFETCH(QString, sourceComponent);
        QFETCH(QString, sourceAction);
        QFETCH(QString, targetComponent);
        QFETCH(QString, targetAction);

        const QString configFileName = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kglobalshortcutsrc");
        QFile::copy(QFINDTESTDATA(initialConfig), configFileName);

        run({
            QStringLiteral("move"),
            QStringLiteral("--config=") + configFileName,
            QStringLiteral("--source-component=") + sourceComponent,
            QStringLiteral("--source-action=") + sourceAction,
            QStringLiteral("--target-component=") + targetComponent,
            QStringLiteral("--target-action=") + targetAction,
        });

        // Compare actual with expected config
        KConfig actual(QStringLiteral("kglobalshortcutsrc"));
        KConfig expected(QFINDTESTDATA(resultConfig));

        compareGroupList(actual, expected);
        compareGroupList(actual.group(QStringLiteral("services")), expected.group(QStringLiteral("services")));
    }

private:
    void compareGroups(const KConfigGroup &a, const KConfigGroup &b)
    {
        for (auto [key, value] : a.entryMap().asKeyValueRange()) {
            QCOMPARE(value, b.readEntry(key));
        }

        for (auto [key, value] : b.entryMap().asKeyValueRange()) {
            QCOMPARE(value, a.readEntry(key));
        }
    }

    void compareGroupList(const KConfigBase &a, const KConfigBase &b)
    {
        QStringList sortedA = a.groupList();
        sortedA.sort();

        QStringList sortedB = b.groupList();
        sortedB.sort();

        QCOMPARE(sortedA, sortedB);

        const QStringList groups = a.groupList();
        for (const QString &group : groups) {
            compareGroups(a.group(group), b.group(group));
        }
    }

    void run(const QStringList &arguments)
    {
        QProcess process;
        process.setProcessChannelMode(QProcess::ForwardedChannels);
        process.setProgram(QCoreApplication::applicationDirPath() + QLatin1String("/kglobalaccel-migrate"));
        process.setArguments(arguments);
        process.start();
        process.waitForFinished();
    }
};

QTEST_MAIN(MigrateConfigTest)

#include "migratetest.moc"
