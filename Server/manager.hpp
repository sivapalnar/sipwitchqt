/*
 * Copyright 2017 Tycho Softworks.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MANAGER_HPP_
#define MANAGER_HPP_

#include "../Common/compiler.hpp"
#include "../Database/authorize.hpp"
#include "invite.hpp"
#include <QMutex>
#include <QCryptographicHash>

class Manager final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Manager)

public:
    inline static Manager *instance() {
        Q_ASSERT(Instance != nullptr);
        return Instance;
    }

    inline static const UString realm() {
        return ServerRealm;
    }

    static const QByteArray computeDigest(const UString &id, const UString &secret, QCryptographicHash::Algorithm digest = QCryptographicHash::Md5);
    static void create(const QList<QHostAddress>& list, quint16 port, unsigned mask);
    static void create(const QHostAddress& addr, quint16 port, unsigned mask);
    static void init(unsigned order);

private:
    static QStringList ServerAliases, ServerNames;
    static UString ServerHostname;
    static UString ServerMode;
    static UString ServerRealm;
    static Manager *Instance;
    static unsigned Contexts;
    static QThread::Priority Priority;

    void applyNames();

    Manager(unsigned order = 0);
    ~Manager() final;

signals:
    void changeRealm(const QString& realm);
    void findEndpoint(const Event& ev);

public slots:
    void refreshRegistration(const Event& ev);
    void createRegistration(const Event& ev, const QVariantHash& endpoint);

    void applyConfig(const QVariantHash& config);

#ifndef QT_NO_DEBUG
    void reportCounts(const QString& id, int count);
#endif
};

/*!
 * The stack is used to manage all sip activities.
 * \file manager.hpp
 */

/*!
 * \class Manager
 * \brief Master sip stack management class.
 * This is used to coordinate all sip activity and runs in it's own thread.
 * This class is meant to be derived into an application specific manager
 * class to introduce product specific behaviors.  This class directly manages
 * the call, registry, and provider objects.  All context events get signaled
 * to here as well.  By having a separate thread and event loop, and signaling
 * all actions through here (or the derived class), correct order and
 * synchronization of object and state changes is guaranteed without locking.
 * \author David Sugar <tychosoft@gmail.com>
 */

#endif
