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

#include "../Common/compiler.hpp"
#include "../Common/server.hpp"
#include "../Common/logging.hpp"
#include "manager.hpp"
#include "main.hpp"

#include <QUuid>

static QHash<QCryptographicHash::Algorithm,QByteArray> digests = {
    {QCryptographicHash::Md5,       "MD5"},
    {QCryptographicHash::Sha256,    "SHA-256"},
    {QCryptographicHash::Sha512,    "SHA-512"},
};

QString Manager::ServerMode;
QString Manager::ServerHostname;
Manager *Manager::Instance = nullptr;
QString Manager::UserAgent;
QString Manager::ServerRealm;
QStringList Manager::ServerAliases;
QStringList Manager::ServerNames;
QCryptographicHash::Algorithm Manager::Digest = QCryptographicHash::Md5;
unsigned Manager::Contexts = 0;

Manager::Manager(unsigned order)
{
    Q_ASSERT(Instance == nullptr);
    Instance = this;

    qRegisterMetaType<Event>("Event");

    moveToThread(Server::createThread("stack", order));
    UserAgent = qApp->applicationName() + "/" + qApp->applicationVersion();
    osip_trace_initialize_syslog(TRACE_LEVEL0, const_cast<char *>(Server::name()));

    Server *server = Server::instance();
    Database *db = Database::instance();

    connect(thread(), &QThread::finished, this, &QObject::deleteLater);
    connect(server, &Server::changeConfig, this, &Manager::applyConfig);

#ifndef QT_NO_DEBUG
    connect(db, &Database::countResults, this, &Manager::reportCounts);
#endif
}

Manager::~Manager()
{
    Instance = nullptr;
}

void Manager::init(unsigned order)
{
    Q_ASSERT(Manager::Instance == nullptr);
    new Manager(order);
}

#ifndef QT_NO_DEBUG
void Manager::reportCounts(const QString& id, int count)
{
    qDebug() << "*** DB Count" << id << count;
}
#endif

void Manager::applyNames()
{
    QStringList names =  ServerAliases + ServerNames;
    qDebug() << "Apply names" << names;
    foreach(auto context, Context::contexts()) {
        context->setHostnames(names, ServerHostname);
    }
}

void Manager::applyConfig(const QVariantHash& config)
{
    ServerNames = config["localnames"].toStringList();
    QString digest = config["digest"].toString().toLower();
    if(digest == "sha2" || digest == "sha256" || digest == "sha-256")
        Digest = QCryptographicHash::Sha256;
    else if(digest == "sha512" || digest == "sha-512")
        Digest = QCryptographicHash::Sha512;
    QString hostname = config["host"].toString();
    QString realm = config["realm"].toString();
    bool genpwd = false;
    if(realm.isEmpty()) {
        genpwd = true;
        realm = Server::sym(CURRENT_NETWORK);
        if(realm.isEmpty() || realm == "local" || realm == "localhost" || realm == "localdomain")
            realm = Server::uuid();
    }
    if(hostname != ServerHostname) {
        ServerHostname = hostname;
        Logging::info() << "starting as host " << ServerHostname;
    }
    if(realm != ServerRealm) {
        ServerRealm = realm;
        Logging::info() << "entering realm " << ServerRealm;
        emit changeRealm(ServerRealm);
    }
    applyNames();
}

const QByteArray Manager::digestName()
{
    return digests.value(Digest, "");
}

const QByteArray Manager::computeDigest(const QString& id, const QString& secret)
{
    if(secret.isEmpty() || id.isEmpty())
        return QByteArray();

    return QCryptographicHash::hash(id.toUtf8() + ":" + realm() + ":" + secret.toUtf8(), Digest);
}

void Manager::create(const QHostAddress& addr, int port, unsigned mask)
{
    unsigned index = ++Contexts;

    qDebug().nospace() << "Creating sip" << index << " " <<  addr << ", port=" << port << ", mask=" << QString("0x%1").arg(mask, 8, 16, QChar('0'));

    foreach(auto schema, Context::schemas()) {
        if(schema.proto & mask) {
            new Context(addr, port, schema, mask, index);
        }
    }
}

void Manager::create(const QList<QHostAddress>& list, int port, unsigned  mask)
{
    foreach(auto host, list) {
        create(host, port, mask);
    }
}

void Manager::registry(const Event &ev)
{
    Registry::events(ev);
}


