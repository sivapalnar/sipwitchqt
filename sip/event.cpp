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

#include "context.hpp"

#ifndef SESSION_EXPIRES
#define SESSION_EXPIRES "session-expires"
#endif

Event::Data::Data() :
expires(-1), status(0), hops(0), natted(false), local(false), context(nullptr), event(nullptr), message(nullptr), authorization(nullptr), association(NONE)
{
}

Event::Data::Data(eXosip_event_t *evt, Context *ctx) :
expires(-1), status(0), hops(0), natted(false), local(false), context(ctx), event(evt), message(nullptr), authorization(nullptr), association(NONE)
{
    // ignore constructor parser if empty event;
    if(!evt) {
        context = nullptr;
        return;
    }

    // start of event decompose by event type...
    switch(evt->type) {
    case EXOSIP_MESSAGE_ANSWERED:
    case EXOSIP_REGISTRATION_SUCCESS:       // provider succeeded
    case EXOSIP_REGISTRATION_FAILURE:       // provider failed
        status = evt->response->status_code;
        reason = evt->response->reason_phrase;
        parseMessage(evt->response);
        break;
    case EXOSIP_MESSAGE_NEW:
    case EXOSIP_CALL_INVITE:
        method = QString(evt->request->sip_method).toUpper();
        if(osip_message_get_authorization(evt->request, 0, &authorization) != 0 || !authorization->username || !authorization->response)
            authorization = nullptr;
        parseMessage(evt->request);
        if(evt->request->req_uri && evt->request->req_uri->host) {
            request = evt->request->req_uri;
            target = Contact(ctx->hostname(), ctx->port(), request.user());
            local = ctx->isLocal(request.host());
        }
        break;
    default:
        break;
    }

    // parse out authorization for later use
    if(authorization && authorization->username)
        userid = Util::removeQuotes(authorization->username);
    if(authorization && authorization->response)
        digest = Util::removeQuotes(authorization->response);
    if(authorization && authorization->nonce)
        nonce = Util::removeQuotes(authorization->nonce);
    if(authorization && authorization->realm)
        realm = Util::removeQuotes(authorization->realm);
    if(authorization && authorization->algorithm)
        algorithm = Util::removeQuotes(authorization->algorithm);
}

Event::Data::~Data()
{
    if(event) {
        eXosip_event_free(event);
        event = nullptr;
    }
}

void Event::Data::parseMessage(osip_message_t *msg)
{
    const osip_list_t& clist = msg->contacts;
    const osip_list_t& vlist = msg->vias;
    int pos;
    osip_via_t *via;
    Contact nat;

    message = msg;
    if(!msg)
        return;

    pos = 0;
    while(osip_list_eol(&vlist, pos) == 0) {
        via = (osip_via_t *)osip_list_get(&vlist, pos++);
        ++hops;
        if(via->host) {
            const char *addr = via->host;
            quint16 port = context->defaultPort(), rport = 0;
            if(via->port)
                port = atoi(via->port);
            osip_uri_param_t *param = nullptr;
            osip_via_param_get_byname(via, (char *)"rport", &param);
            if(param && param->gvalue)
                rport = atoi(param->gvalue);
            if(via->port)
                port = atoi(via->port);
            source = Contact(addr, port);

            // top nat only
            if(!nat && rport) {
                param = nullptr;
                osip_via_param_get_byname(via, (char *)"received", &param);
                if(param && param->gvalue)
                    nat = Contact(param->gvalue, rport);
            }
        }
    }

    // nat overrides last via contact source address
    if(nat) {
        natted = true;
        source = nat;
    }

    osip_header_t *header;

    header = nullptr;
    osip_message_header_get_byname(msg, USER_AGENT, 0, &header);
    if(header && header->hvalue)
        agent = header->hvalue;

    header = nullptr;
    osip_message_header_get_byname(msg, SESSION_EXPIRES, 0, &header);
    if(header && header->hvalue)
        expires = atoi(header->hvalue);

    header = nullptr;
    osip_message_get_subject(msg, 0, &header);
    if(header && header->hvalue)
        subject = header->hvalue;

    pos = 0;
    while(osip_list_eol(&clist, pos) == 0) {
        auto contact = (osip_contact_t *)osip_list_get(&clist, pos++);
        if(contact && contact->url)
            contacts << Contact(contact);
    }

    if(msg->from)
        from = msg->from->url;
    if(msg->to)
        to = msg->to->url;

    if(msg->content_type && msg->content_type->type) {
        content = msg->content_type->type;
        if(msg->content_type->subtype)
            content += QString("/") + msg->content_type->subtype;
    }

    osip_body_t *data = nullptr;
    osip_message_get_body(msg, 0, &data);
    if(data && data->length)
        body = QByteArray(data->body, data->length);
}

Event::Event()
{
    d = new Event::Data();
}

Event::Event(eXosip_event_t *evt, Context *ctx)
{
    d = new Event::Data(evt, ctx);
}

Event::Event(const Event& copy) :
d(copy.d)
{
}

Event::~Event()
{
}

const QString Event::text() const
{
    QString result;

    if(!d->message)
        return result;

    char *data = nullptr;
    size_t len;
    osip_message_to_str(d->message, &data, &len);
    if(data) {
        data[len] = 0;
        result = data;
        osip_free(data);
    }
    return result;
}

const QString Event::uriContext(const QString& username) const
{
    if(!d->context)
        return "";

    return d->context->uriFrom(username);
}

// used for events that support only one contact object...
const Contact Event::contact() const
{
    if(d->contacts.size() != 1)
        return Contact();
    return d->contacts[0];
}

const QString Event::protocol() const
{
    Q_ASSERT(d->context != nullptr);
    return d->context->type();
}

inline const QString Event::uri(const Contact& addr) const {
    Q_ASSERT(d->context != nullptr);
    return d->context->uriTo(addr);
}

QDebug operator<<(QDebug dbg, const Event& ev)
{
    if(ev) {
        if(ev.status())
            dbg.nospace() << "Event(" << ev.toString() << ",cid=" << ev.cid() << ",did=" << ev.did() << ",ctx=" << ev.context()->objectName() << ",source=" << ev.source().toString() << ",status=" << ev.status() << ")";
        else
            dbg.nospace() << "Event(" << ev.toString() << ",cid=" << ev.cid() << ",did=" << ev.did() << ",ctx=" << ev.context()->objectName() << ",source=" << ev.source().toString() << ",target=" << ev.target().toString() << ")";
    }
    else
        dbg.nospace() << "Event(timeout)";
    return dbg.maybeSpace();
}

const QString Event::toString() const
{
    if(!d->event)
        return "none";

    QString result = QString::number(d->event->type);
    switch(d->event->type) {
    case EXOSIP_REGISTRATION_SUCCESS:
        return result + "/register";
    case EXOSIP_CALL_INVITE:
        return result + "/invite";
    case EXOSIP_CALL_REINVITE:
        return result + "/reinvite";
    case EXOSIP_CALL_NOANSWER:
    case EXOSIP_SUBSCRIPTION_NOANSWER:
    case EXOSIP_NOTIFICATION_NOANSWER:
        return result + "/noanswer";
    case EXOSIP_MESSAGE_PROCEEDING:
    case EXOSIP_NOTIFICATION_PROCEEDING:
    case EXOSIP_CALL_MESSAGE_PROCEEDING:
    case EXOSIP_SUBSCRIPTION_PROCEEDING:
    case EXOSIP_CALL_PROCEEDING:
        return result + "/proceed";
    case EXOSIP_CALL_RINGING:
        return result + "/ring";
    case EXOSIP_MESSAGE_ANSWERED:
    case EXOSIP_CALL_ANSWERED:
    case EXOSIP_CALL_MESSAGE_ANSWERED:
    case EXOSIP_SUBSCRIPTION_ANSWERED:
    case EXOSIP_NOTIFICATION_ANSWERED:
        return result + "/answer";
    case EXOSIP_SUBSCRIPTION_REDIRECTED:
    case EXOSIP_NOTIFICATION_REDIRECTED:
    case EXOSIP_CALL_MESSAGE_REDIRECTED:
    case EXOSIP_CALL_REDIRECTED:
    case EXOSIP_MESSAGE_REDIRECTED:
        return result + "/redirect";
    case EXOSIP_REGISTRATION_FAILURE:
        return result + "/noreg";
    case EXOSIP_SUBSCRIPTION_REQUESTFAILURE:
    case EXOSIP_NOTIFICATION_REQUESTFAILURE:
    case EXOSIP_CALL_REQUESTFAILURE:
    case EXOSIP_CALL_MESSAGE_REQUESTFAILURE:
    case EXOSIP_MESSAGE_REQUESTFAILURE:
        return result + "/failed";
    case EXOSIP_SUBSCRIPTION_SERVERFAILURE:
    case EXOSIP_NOTIFICATION_SERVERFAILURE:
    case EXOSIP_CALL_SERVERFAILURE:
    case EXOSIP_CALL_MESSAGE_SERVERFAILURE:
    case EXOSIP_MESSAGE_SERVERFAILURE:
        return result + "/server";
    case EXOSIP_SUBSCRIPTION_GLOBALFAILURE:
    case EXOSIP_NOTIFICATION_GLOBALFAILURE:
    case EXOSIP_CALL_GLOBALFAILURE:
    case EXOSIP_CALL_MESSAGE_GLOBALFAILURE:
    case EXOSIP_MESSAGE_GLOBALFAILURE:
        return result + "/global";
    case EXOSIP_CALL_ACK:
        return result + "/ack";
    case EXOSIP_CALL_CLOSED:
    case EXOSIP_CALL_RELEASED:
        return result + "/bye";
    case EXOSIP_CALL_CANCELLED:
        return result + "/cancel";
    case EXOSIP_MESSAGE_NEW:
    case EXOSIP_CALL_MESSAGE_NEW:
    case EXOSIP_IN_SUBSCRIPTION_NEW:
        return result + "/new";
    case EXOSIP_SUBSCRIPTION_NOTIFY:
        return result + "/notify";
    default:
        break;
    }
    return "unknown";
}

namespace Util {
    QString removeQuotes(const QString& str)
    {
        if(str.startsWith("\"") && str.endsWith("\""))
            return str.mid(1, str.length() - 2);
        else if(str.startsWith("'") && str.endsWith("'"))
            return str.mid(1, str.length() - 2);
        return str;
    }
};
