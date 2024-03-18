/*
    This file is part of the KDE libraries
    SPDX-FileCopyrightText: 2001, 2002 Ellis Whitehead <ellis@kde.org>
    SPDX-FileCopyrightText: 2013 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kglobalaccel_test.h"

#include "logging_p.h"

#include <QApplication>
#include <QWidget>
#include <private/qtx11extras_p.h>

//----------------------------------------------------

KGlobalAccelInterface *s_interface;

KGlobalAccelImpl::KGlobalAccelImpl(QObject *parent)
    : KGlobalAccelInterface(parent)
{
    qCritical() << "myfix: KGlobalAccelImpl::KGlobalAccelImpl";
}

KGlobalAccelImpl::~KGlobalAccelImpl()
{
}

bool KGlobalAccelImpl::grabKey(int keyQt, bool grab)
{
    return true;
}

void KGlobalAccelImpl::setEnabled(bool enable)
{
    qCritical() << "myfix: KGlobalAccelImpl::setEnabled" << enable;
    s_interface = enable ? this : nullptr;
    qCritical() << "myfix: KGlobalAccelImpl::setEnabled" << s_interface;
}

bool KGlobalAccelImpl::checkKeyPressed(int keyQt)
{
    return keyPressed(keyQt);
}

bool KGlobalAccelImpl::checkKeyReleased(int keyQt)
{
    return keyReleased(keyQt);
}
