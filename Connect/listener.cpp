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

#include "listener.hpp"

#include <QTimer>

#if defined(Q_OS_WIN)
#include <WinSock2.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static const char *eid(eXosip_event_type ev);

static QHash<UString, QCryptographicHash::Algorithm> digests = {
    {"MD5",     QCryptographicHash::Md5},
    {"SHA",     QCryptographicHash::Sha1},
    {"SHA1",    QCryptographicHash::Sha1},
    {"SHA2",    QCryptographicHash::Sha256},
    {"SHA256",  QCryptographicHash::Sha256},
    {"SHA512",  QCryptographicHash::Sha512},
    {"SHA-1",   QCryptographicHash::Sha1},
    {"SHA-256", QCryptographicHash::Sha256},
    {"SHA-512", QCryptographicHash::Sha512},
};

// internal lock class
class Locker final
{
public:
    inline Locker(eXosip_t *ctx) :
    context(ctx) {
        Q_ASSERT(context != nullptr);
        eXosip_lock(context);
    }

    inline ~Locker() {
        eXosip_unlock(context);
    }

private:
    eXosip_t *context;
};

Listener::Listener(const QVariantHash& cred, const QSslCertificate& cert) :
QObject(), active(true), connected(false), registered(false)
{
    serverId = cred["extension"].toString();
    serverHost = cred["server"].toString();
    serverPort = static_cast<quint16>(cred["port"].toUInt());
    serverInit = cred["initialize"].toString();
    serverLabel = cred["label"].toString();
    serverSchema = "sip:";
    serverCreds = cred;
    serverCreds["initialize"] = "";

    if(!serverPort)
        serverPort = 5060;

    family = AF_INET;
    tls = 0;
    rid = -1;

    if(!cert.isNull()) {
        serverSchema = "sips:";
        ++tls;
    }

    if(!serverPort) {
        serverPort = 5060;
        if(tls)
            ++serverPort;
    }

    // timer has to be connected before we move listener to it's own thread...
    // this is required for udp
    QTimer::singleShot(5000, this, &Listener::timeout);
    QThread *thread = new QThread;
    this->moveToThread(thread);

    connect(thread, &QThread::started, this, &Listener::run);
    connect(this, &Listener::finished, thread, &QThread::quit);
    connect(this, &Listener::finished, this, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
}

void Listener::send_registration(osip_message_t *msg, bool auth)
{
    if(!msg || rid < 0)
        return;

    // add generic headers...
    osip_message_set_header(msg, "Allow", AGENT_ALLOWS);

    // add our special registration headers...
    osip_message_set_header(msg, "X-Label", serverLabel);
    if(!serverInit.isEmpty())
        osip_message_set_header(msg, "X-Initialize", serverInit);

    if(auth)
        serverInit = "";

    eXosip_register_send_register(context, rid, msg);
}

void Listener::reauthorize(const QVariantHash& update)
{
    Locker lock(context);
    foreach(auto key, update.keys())
        serverCreds[key] = update[key];

    if(rid < 0 || !active)
        return;

    osip_message_t *msg = nullptr;
    eXosip_register_build_register(context, rid, AGENT_EXPIRES, &msg);
    if(msg)
        send_registration(msg);
    else
        active = false;
}

bool Listener::get_authentication(eXosip_event_t *event)
{
    if(!event->response)
        return false;

    auto pauth = static_cast<osip_proxy_authenticate_t*>(osip_list_get(&event->response->proxy_authenticates, 0));
    auto wauth = static_cast<osip_proxy_authenticate_t*>(osip_list_get(&event->response->www_authenticates,0));

    osip_header_t *header = nullptr;
    osip_message_header_get_byname(event->response, "x-authorize", 0, &header);
    if(header && header->hvalue)
        serverCreds["user"] = QString(header->hvalue);

    if(!pauth && !wauth)
        return false;

    UString realm, algo, nonce, user, secret, auth;
    if(pauth) {
        realm = osip_proxy_authenticate_get_realm(pauth);
        algo = osip_proxy_authenticate_get_algorithm(pauth);
        nonce = osip_proxy_authenticate_get_nonce(pauth);
    }
    else {
        realm = osip_www_authenticate_get_realm(wauth);
        algo = osip_www_authenticate_get_algorithm(wauth);
        nonce = osip_www_authenticate_get_nonce(wauth);
    }
    serverCreds["realm"] = realm.unquote();
    serverCreds["nonce"] = nonce.unquote();
    serverCreds["algorithm"] = algo.unquote().toUpper();
    return true;
}

void Listener::add_authentication(osip_message_t *msg)
{
    UString user = serverCreds["user"].toString();
    UString secret = serverCreds["secret"].toString();
    UString realm = serverCreds["realm"].toString();
    UString algo = serverCreds["algorithm"].toString();
    UString nonce = serverCreds["nonce"].toString();

    auto digest = digests[algo];
    char *uri = nullptr;
    osip_uri_to_str(msg->req_uri, &uri);

    UString method = msg->sip_method;
    UString ha1 = QCryptographicHash::hash(user + ":" + realm + ":" + secret, digest).toHex().toLower();
    UString ha2 = QCryptographicHash::hash(method + ":" + uri, digest).toHex().toLower();
    UString response = QCryptographicHash::hash(ha1 + ":" + nonce + ":" + ha2, digest).toHex().toLower();
    UString auth = "Digest username=\"" + user +
        "\", realm=\"" + realm +
        "\", uri=\"" + uri +
        "\", response=\"" + response +
        "\", nonce=\"" + nonce +
        "\", algorithm=\"" + algo;

    osip_message_set_header(msg, AUTHORIZATION, auth);
}

void Listener::run()
{
    int ipv6 = 0, rport = 1, dns = 2;

#ifdef AF_INET6
    if(family == AF_INET && serverHost.contains(':')) {
        family = AF_INET6;
        ++ipv6;
    }
#endif

    context = eXosip_malloc();
    eXosip_init(context);
    eXosip_set_option(context, EXOSIP_OPT_ENABLE_IPV6, &ipv6);
    eXosip_set_option(context, EXOSIP_OPT_USE_RPORT, &rport);
    eXosip_set_option(context, EXOSIP_OPT_DNS_CAPABILITIES, &dns);
    eXosip_set_user_agent(context, UString("SipWitchQt-client/") + PROJECT_VERSION);
    eXosip_listen_addr(context, IPPROTO_TCP, NULL, 0, family, tls);

    emit starting();

    if(active) {
        Locker lock(context);
        osip_message_t *msg = nullptr;
        auto identity = UString::uri(serverSchema, serverId, serverHost, serverPort);
        auto server = UString::uri(serverSchema, serverHost, serverPort);
        qDebug() << "Connecting to" << server;
        qDebug() << "Connecting as" << identity;

        rid = eXosip_register_build_initial_register(context, identity, server, NULL, AGENT_EXPIRES, &msg);
        if(msg && rid > -1)
            send_registration(msg);
        else
            active = false;
    }

    int s = EVENT_TIMER / 1000l;
    int ms = EVENT_TIMER % 1000l;
    int error;

    while(active) {
        auto event = eXosip_event_wait(context, s, ms);
        if(!active)
            break;

        // timeout...
        if(!event) {
            qDebug() << "timeout";
            Locker lock(context);
            eXosip_automatic_action(context);
            continue;
        }
        else {
            connected = true;
            qDebug().nospace() << "type=" << eid(event->type) << " cid=" << event->cid;
        }

        switch(event->type) {
        case EXOSIP_REGISTRATION_SUCCESS:
            registered = true;
            emit authorize(serverCreds);
            break;
        case EXOSIP_REGISTRATION_FAILURE:
            if(event->rid != rid)
                break;

            error = 666;
            if(event->response)
                error = event->response->status_code;

            if(error != SIP_UNAUTHORIZED) {
                active = false;
                emit failure(error);
                break;
            }
            if(get_authentication(event)) {
                osip_message_t *msg = nullptr;
                Locker lock(context);
                eXosip_register_build_register(context, rid, AGENT_EXPIRES, &msg);
                if(msg) {
                    add_authentication(msg);
                    send_registration(msg, true);
                    break;
                }
            }
            active = false;
            emit failure(SIP_FORBIDDEN);
            break;
        case EXOSIP_CALL_MESSAGE_NEW:
            if(MSG_IS_REGISTER(event->request)) {
                Locker lock(context);
                eXosip_message_send_answer(context, event->tid, SIP_METHOD_NOT_ALLOWED, nullptr);
                break;
            }
            break;
        default:
            break;
        }

        eXosip_event_free(event);
    }

    // de-register if we are ending the session while registered
    if(registered) {
        osip_message_t *msg = nullptr;
        Locker lock(context);
        eXosip_register_build_register(context, rid, 0, &msg);
        if(msg)
            send_registration(msg);
    }

    // clean up exiting transactions...
    while(registered) {
        auto event = eXosip_event_wait(context, 0, 60);
        if(event == nullptr)
            break;
        else {
            switch(event->type) {
            case EXOSIP_REGISTRATION_SUCCESS:
                registered = false;
                break;
            case EXOSIP_REGISTRATION_FAILURE:
                if(get_authentication(event)) {
                    osip_message_t *msg = nullptr;
                    Locker lock(context);
                    eXosip_register_build_register(context, rid, 0, &msg);
                    if(msg) {
                        add_authentication(msg);
                        send_registration(msg);
                        registered = false;
                        break;
                    }
                }
                registered = false;
                break;
            default:
                break;
            }
        }
        eXosip_event_free(event);
    }

    emit finished();
    eXosip_quit(context);
    context = nullptr;
}

void Listener::timeout()
{
    if(connected)
        return;

    active = false;
    emit failure(666);  // special code for unable to reach server...
}

void Listener::stop()
{
    active = false;
    QThread::msleep(650);
}

static const char *eid(eXosip_event_type ev)
{
    switch(ev) {
    case EXOSIP_REGISTRATION_SUCCESS:
        return "register";
    case EXOSIP_CALL_INVITE:
        return "invite";
    case EXOSIP_CALL_REINVITE:
        return "reinvite";
    case EXOSIP_CALL_NOANSWER:
    case EXOSIP_SUBSCRIPTION_NOANSWER:
    case EXOSIP_NOTIFICATION_NOANSWER:
        return "noanswer";
    case EXOSIP_MESSAGE_PROCEEDING:
    case EXOSIP_NOTIFICATION_PROCEEDING:
    case EXOSIP_CALL_MESSAGE_PROCEEDING:
    case EXOSIP_SUBSCRIPTION_PROCEEDING:
    case EXOSIP_CALL_PROCEEDING:
        return "proceed";
    case EXOSIP_CALL_RINGING:
        return "ring";
    case EXOSIP_MESSAGE_ANSWERED:
    case EXOSIP_CALL_ANSWERED:
    case EXOSIP_CALL_MESSAGE_ANSWERED:
    case EXOSIP_SUBSCRIPTION_ANSWERED:
    case EXOSIP_NOTIFICATION_ANSWERED:
        return "answer";
    case EXOSIP_SUBSCRIPTION_REDIRECTED:
    case EXOSIP_NOTIFICATION_REDIRECTED:
    case EXOSIP_CALL_MESSAGE_REDIRECTED:
    case EXOSIP_CALL_REDIRECTED:
    case EXOSIP_MESSAGE_REDIRECTED:
        return "redirect";
    case EXOSIP_REGISTRATION_FAILURE:
        return "noreg";
    case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:
    case EXOSIP_NOTIFICATION_REQUESTFAILURE:
    case EXOSIP_CALL_REQUESTFAILURE:
    case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
    case EXOSIP_MESSAGE_REQUESTFAILURE:
        return "failed";
    case EXOSIP_SUBSCRIPTION_SERVERFAILURE:
    case EXOSIP_NOTIFICATION_SERVERFAILURE:
    case EXOSIP_CALL_SERVERFAILURE:
    case EXOSIP_CALL_MESSAGE_SERVERFAILURE:
    case EXOSIP_MESSAGE_SERVERFAILURE:
        return "server";
    case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:
    case EXOSIP_NOTIFICATION_GLOBALFAILURE:
    case EXOSIP_CALL_GLOBALFAILURE:
    case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:
    case EXOSIP_MESSAGE_GLOBALFAILURE:
        return "global";
    case EXOSIP_CALL_ACK:
        return "ack";
    case EXOSIP_CALL_CLOSED:
    case EXOSIP_CALL_RELEASED:
        return "bye";
    case EXOSIP_CALL_CANCELLED:
        return "cancel";
    case EXOSIP_MESSAGE_NEW:
    case EXOSIP_CALL_MESSAGE_NEW:
    case EXOSIP_IN_SUBSCRIPTION_NEW:
        return "new";
    case EXOSIP_SUBSCRIPTION_NOTIFY:
        return "notify";
    default:
        break;
    }
    return "unknown";
}
