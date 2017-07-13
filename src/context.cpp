/**
 ** Copyright 2017 Tycho Softworks.
 **
 ** This program is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 3 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/

#include "server.hpp"
#include "logging.hpp"
#include "control.hpp"
#include "stack.hpp"

#include <QNetworkInterface>

#ifdef Q_OS_UNIX
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#ifdef Q_OS_WIN
#include <WinSock2.h>
#pragma comment(lib, "ssleay32.lib")
#pragma comment(lib, "libeay32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "gdi32.lib")
#endif

#define EVENT_TIMER 500l    // 500ms...

static volatile unsigned instanceCount = 0;
static bool active = true;

QList<Context *> Context::Contexts;
QList<Context::Schema> Context::Schemas = {
    {"udp", "sip:",  Context::UDP, 5060, IPPROTO_UDP},
    {"tcp", "sip:",  Context::TCP, 5060, IPPROTO_TCP},
    {"tls", "sips:", Context::TLS, 5061, IPPROTO_TCP},
    {"dtls","sips:", Context::DTLS, 5061, IPPROTO_UDP},
};

// internal lock class
class ContextLocker final
{
public:
    inline ContextLocker(eXosip_t *ctx) :
    context(ctx) {
        Q_ASSERT(context != nullptr);
        eXosip_lock(context);
    }

    inline ~ContextLocker() {
        eXosip_unlock(context);
    }

private:
    eXosip_t *context;
};


Context::Context(const QHostAddress& addr, int port, const Schema& choice, unsigned mask, unsigned index):
schema(choice), context(nullptr), netFamily(AF_INET), netPort(port)
{
    allow = mask & 0xffffff00;
    netPort &= 0xfffe;
    netPort |= (schema.inPort & 0x01);
    netProto = schema.inProto;
    netTLS = (schema.inPort & 0x01);

    context = eXosip_malloc();
    eXosip_init(context);
    eXosip_set_user_agent(context, Stack::agent());

    auto proto = addr.protocol();
    int ipv6 = 0, rport = 1, dns = 2;
    if(proto == QAbstractSocket::IPv6Protocol) {
        netFamily = AF_INET6;
        ipv6 = 1;
    } 
    eXosip_set_option(context, EXOSIP_OPT_ENABLE_IPV6, &ipv6);
    eXosip_set_option(context, EXOSIP_OPT_USE_RPORT, &rport);
    eXosip_set_option(context, EXOSIP_OPT_DNS_CAPABILITIES, &dns);

    if(!addr.isNull())
        netAddress = addr.toString().toUtf8();

    setObjectName(QString("sip") + QString::number(index) + "/" + choice.name);
    Contexts << this;

    if(addr != QHostAddress::Any && addr != QHostAddress::AnyIPv4 && addr != QHostAddress::AnyIPv6) {
        if(ipv6)
            uriAddress = "[" + netAddress + "]";
        else
            uriAddress = netAddress;
    }
    else {
        uriAddress = QHostInfo::localHostName();
    }

    // reverse lookup so we can use interface name rather than addr...
    if(!netAddress.isEmpty() && netAddress != "0.0.0.0") {
        QHostInfo host = QHostInfo::fromName(netAddress);
        if(host.error() == QHostInfo::NoError)
            localHosts << host.hostName();
        localHosts << netAddress;
    }
    localHosts << QHostInfo::localHostName() << Util::localDomain();

    if(netPort != schema.inPort)
        uriAddress += ":" + QString::number(netPort);

    //qDebug() << "****** URI TO " << uriTo(QHostAddress("4.2.2.1"));
    //qDebug() << "**** LOCAL URI" << uri();
}

Context::~Context()
{
    if(context)
        eXosip_quit(context);
}

QAbstractSocket::NetworkLayerProtocol Context::protocol()
{
    if(netFamily == AF_INET)
        return QAbstractSocket::IPv4Protocol;
    else
        return QAbstractSocket::IPv6Protocol;
}

void Context::run()
{
    ++instanceCount;

    // connect events to state handlers when we run...
    auto stack = Stack::instance();
    if(allow & Allow::REGISTRY)
        connect(this, &Context::registry, stack, &Stack::registry);

    Logging::debug() << "Running " << objectName();

    const char *ap = nullptr;

    if(!netAddress.isEmpty()) {
        ap = netAddress.constData();
    }
    else if(netAddress == "0.0.0.0")
        ap = nullptr;

    priorEvent = 0;
    
    //qDebug() << "LISTEN " << proto << ap << port << family << NetTLS;
    if(eXosip_listen_addr(context, netProto, ap, netPort, netFamily, netTLS)) {
        Logging::err() << objectName() << ": failed to bind and listen";
        context = nullptr;
    }

    while(active && context) {
        int s = EVENT_TIMER / 1000l;
        int ms = EVENT_TIMER % 1000l;
        Event event(eXosip_event_wait(context, s, ms), this);
        time(&currentEvent);
        if(currentEvent != priorEvent) {
            priorEvent = currentEvent;
            ContextLocker lock(context);    // scope lock automatic block...
            eXosip_automatic_action(context);
        }
        if(!event)
            continue;

        // skip extra code in event loop if we don't need it...
        if(Server::verbose())
            Logging::debug() << event;

        if(Server::state() == Server::UP && process(event))
            continue;
    }
    Logging::debug() << "Exiting " << objectName();
    emit finished();
    --instanceCount;
}

bool Context::authenticated(const Event& ev) {
    if(allow && Allow::UNAUTHENTICATED)
        return true;
    if(ev.authorization())
        return true;

    // create challenge...
    osip_message_t *reply = nullptr;
    time_t now;     //TODO: A real nonce generator and cache to verify replies
    QString nonce = QString::number(time(&now));
    QString challenge = QString("Digest Realm=\"") + Stack::realm() + QString("\", nonce=\"") + nonce + QString("\", algorithm=\"") + Stack::digestName() + QString("\"");

    ContextLocker lock(context);
    eXosip_message_build_answer(context, ev.tid(), SIP_UNAUTHORIZED, &reply);
    osip_message_set_header(reply, WWW_AUTHENTICATE, challenge.toUtf8().constData());
    eXosip_message_send_answer(context, ev.tid(), SIP_UNAUTHORIZED, reply);
    return false;
}

bool Context::process(const Event& ev) {
    switch(ev.type()) {
    case EXOSIP_MESSAGE_NEW:
        if(MSG_IS_REGISTER(ev.request())) {
            if(!authenticated(ev))
                return false;
            if(!(allow & Allow::REGISTRY))
                return false;
            emit registry(ev);
        }
        else
            return false;
        break;
    default:
        return false;
    }

    return true;
}

void Context::start(QThread::Priority priority)
{
    foreach(auto context, Contexts) {
        QThread::msleep(20);
        QThread *thread = new QThread;
        thread->setObjectName(context->objectName());
        context->moveToThread(thread);

        connect(thread, &QThread::started, context, &Context::run);
        connect(context, &Context::finished, thread, &QThread::quit);
        connect(context, &Context::finished, context, &QObject::deleteLater);
        connect(thread, &QThread::finished, thread, &QThread::deleteLater);
        thread->start(priority);
    }
    QThread::msleep(100);
    qDebug() << "Started contexts" << instanceCount;
}

void Context::shutdown()
{
    qDebug() << "Shutdown contexts" << instanceCount;
    active = false;

    unsigned hanged = 50;   // up to 5 seconds, after we force...

    while(instanceCount && hanged--) {
        QThread::msleep(100);
    }
}

void Context::setOtherNames(QStringList names) 
{
    QMutexLocker lock(&nameLock);
    otherNames = names;
}

void Context::setPublicName(QString name)
{
    if(!(allow & Allow::REMOTE))
        return;

    QMutexLocker lock(&nameLock);
    publicName = name;
}

const QStringList Context::localnames() const
{
    QStringList names = localHosts;
    QMutexLocker lock(&nameLock);
    names << otherNames;
    if(publicName.length() > 0)
        names << publicName;
    names << QHostInfo::localHostName();
    return names;
}






