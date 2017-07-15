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

#include "inline.hpp"
#include "contact.hpp"

Contact::Contact(const QString& address, quint16 port, const QString& user, int duration) noexcept :
expiration(0), username(user)
{
    pair.first = address;
    pair.second = port;
    if(duration > -1) {
        time(&expiration);
        expiration += duration;
    }
}

Contact::Contact(osip_uri_t *uri)  noexcept :
pair("", 0), expiration(0)
{
    if(!uri->host || uri->host[0] == 0)
        return;

    pair.first = uri->host;
    pair.second = 5060;
    username = uri->username;
    if(uri->port && uri->port[0])
        pair.second = atoi(uri->port);
}

Contact::Contact(osip_contact_t *contact)  noexcept :
pair("", 0), expiration(0)
{
    if(!contact->url)
        return;

    osip_uri_t *uri = contact->url;
    if(!uri->host || uri->host[0] == 0)
        return;

    // see if contact has expiration
    osip_uri_param_t *param = nullptr;
    osip_contact_param_get_byname(contact, (char *)"expires", &param);
    if(param && param->gvalue)
        refresh(osip_atoi(param->gvalue));

    pair.first = uri->host;
    pair.second = 5060;
    username = uri->username;
    if(uri->port && uri->port[0])
        pair.second = atoi(uri->port);
 }

Contact::Contact(const Contact& from) noexcept
{
    pair = from.pair;
    expiration = from.expiration;
    username = from.username;
}

Contact::Contact(Contact&& from) noexcept
{
    pair = std::move(from.pair);
    from.pair.second = 0;
    from.pair.first = "";
    from.expiration = 0;
    from.username = "";
}

Contact::Contact() noexcept :
pair("", 0), expiration(0)
{
}

bool Contact::hasExpired() const {
    if(!expiration)
        return false;
    time_t now;
    time(&now);
    if(now >= expiration)
        return true;
    return false;
}

void Contact::refresh(int seconds) {
    if(seconds < 0) {
        expiration = 0;
        return;
    }

    if(!expiration)
        time(&expiration);
    expiration += seconds;
}


QDebug operator<<(QDebug dbg, const Contact& addr)
{
    time_t now, expireTime = addr.expires();
    time(&now);
    QString expires = "never";
    if(expireTime) {
        if(now >= expireTime)
            expires = "expired";
        else
            expires = QString::number(expireTime - now) + "s";
    }
    dbg.nospace() << "Contact(" << addr.host() << ":" << addr.port() << ",expires=" << expires << ")";
    return dbg.maybeSpace();
}
