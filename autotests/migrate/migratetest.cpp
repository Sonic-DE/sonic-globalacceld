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
    void moveGroup_data()
    {
        QTest::addColumn<QString>("initial");
        QTest::addColumn<QString>("result");

        QTest::addRow("normal") << QStringLiteral("move-group") << QStringLiteral("move-group.expected");
        QTest::addRow("all shortcuts are default") << QStringLiteral("move-group-defaulted") << QStringLiteral("move-group-defaulted.expected");
    }

    void moveGroup()
    {
        QDir configDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation));
        configDir.mkpath(QStringLiteral("."));
        configDir.remove(QStringLiteral("kglobalshortcutsrc"));

        QFETCH(QString, initial);
        QFETCH(QString, result);

        const QString configFileName = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/kglobalshortcutsrc");
        QFile::copy(QFINDTESTDATA(initial), configFileName);

        run({
            QStringLiteral("move"),
            QStringLiteral("--config=") + configFileName,
            QStringLiteral("--source-component=org.kde.spectacle.desktop"),
            QStringLiteral("--target-component=services/org.kde.spectacle.desktop"),
        });

        // Compare actual with expected config
        KConfig actual(QStringLiteral("kglobalshortcutsrc"));
        KConfig expected(QFINDTESTDATA(result));

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
        QCOMPARE(a.groupList(), b.groupList());

        const QStringList groups = a.groupList();
        for (const QString &group : groups) {
            compareGroups(a.group(group), b.group(group));
        }
    }

    void run(const QStringList &arguments)
    {
        const auto ownPath = QCoreApplication::libraryPaths().last();

        QProcess process;
        process.setProcessChannelMode(QProcess::ForwardedChannels);
        process.setProgram(ownPath + QLatin1String("/kglobalaccel-migrate"));
        qDebug() << process.program() << QCoreApplication::applicationDirPath();
        process.setArguments(arguments);
        process.start();
        process.waitForFinished();
    }
};

QTEST_MAIN(MigrateConfigTest)

#include "migratetest.moc"
