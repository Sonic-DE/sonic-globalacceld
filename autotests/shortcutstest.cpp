/*
    SPDX-FileCopyrightText: 2024 Yifan Zhu <fanzhuyifan@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QTest>

#include "dummy.h"
#include "kglobalacceld.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QPluginLoader>
#include <QSignalSpy>

Q_IMPORT_PLUGIN(KGlobalAccelImpl)

class ShortcutsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    void testShortcuts();

private:
    std::unique_ptr<KGlobalAccelD> m_globalaccel;
    KGlobalAccelInterface *m_interface;
};

void ShortcutsTest::initTestCase()
{
    qputenv("KGLOBALACCELD_PLATFORM", "dummy");
    m_globalaccel = std::make_unique<KGlobalAccelD>();
    QVERIFY(m_globalaccel->init());
    // KGlobalAccelImpl *impl = KGlobalAccelImpl::self();
    m_interface = KGlobalAccelImpl::instance();
    qCritical() << "myfix: m_interface" << m_interface;
    QVERIFY(m_interface);
    // create dbus call
    // QDBusInterface iface(QStringLiteral("org.kde.KGlobalAccel"),
    // QStringLiteral("/kglobalaccel"),
    // QStringLiteral("org.kde.kglobalaccel.Component"));
    // QDBusReply<QList<QStringList>> reply =
    // iface.call(QStringLiteral("allComponentNames")); qCritical() <<
    // reply.value()
}

void ShortcutsTest::cleanupTestCase()
{
}

void ShortcutsTest::testShortcuts()
{
    QAction *action = new QAction(this);
    action->setObjectName(QStringLiteral("ActionForShortcutTest"));
    QVERIFY(KGlobalAccel::setGlobalShortcut(action, QKeySequence(Qt::ControlModifier | Qt::Key_P)));
    QSignalSpy spy(action, &QAction::triggered);
    // quint32 timestamp = 1;
    bool retVal;
    QMetaObject::invokeMethod(m_interface, "checkKeyPressed", Qt::DirectConnection, Q_ARG(int, Qt::Key_Control));
    QMetaObject::invokeMethod(m_interface, "checkKeyReleased", Qt::DirectConnection, Q_ARG(int, Qt::Key_P | (int)Qt::ControlModifier));

    QVERIFY(spy.wait());
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(ShortcutsTest)

#include "shortcutstest.moc"
