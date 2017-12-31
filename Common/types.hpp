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

#ifndef TYPES_HPP_
#define TYPES_HPP_

#include "compiler.hpp"
#include <QByteArray>
#include <QString>
#include <QMetaType>

class UString : public QByteArray
{
public:
    UString() : QByteArray("") {}
    UString(const char *string) : QByteArray(string) {}
    UString(const QByteArray& ref) : QByteArray(ref) {}
    UString(const QString& ref) : QByteArray(ref.toUtf8()) {}
    UString(const std::string& ref) : QByteArray(ref.c_str()) {}
    UString(const UString& ref) : QByteArray(ref) {}
    UString(UString&& from) : QByteArray(std::move(from)) {}
    explicit UString(char ch) : QByteArray() { append(ch); }

    operator std::string() const {
        return std::string(constData());
    }

    UString& operator=(const UString& ref) {
        QByteArray::operator=(ref);
        return *this;
    }

    UString& operator=(UString&& from) {
        QByteArray::operator=(from);
        return *this;
    }

    UString unquote(const char *qc = "\"") const;
    UString quote(const char *qc = "\"") const;
    bool isQuoted(const char *qc = "\"") const;
};

inline UString operator+(const UString& str, char *add) {
    UString result(str);
    result.append(add);
    return result;
}

inline UString operator+(const UString& str, const char *add) {
    UString result(str);
    result.append(add);
    return result;
}

inline UString operator+(const UString& str, const std::string& add) {
    UString result(str);
    result.append(add.c_str());
    return result;
}

inline UString operator+(const std::string& str, const UString& add) {
    UString result(str.c_str());
    result.append(add);
    return result;
}

inline UString operator+(const UString& str, const UString& add) {
    UString result(str);
    result.append(add);
    return result;
}

inline UString operator+(const char *str, const UString& add) {
    UString result(str);
    result.append(add);
    return result;
}

inline QString operator+(const QString& str, const UString& add) {
    QString result(str);
    result.append(add);
    return result;
}

inline QString operator+(const UString& str, const QString& add) {
    QString result(str.constData());
    result.append(add.toUtf8());
    return result;
}

Q_DECLARE_METATYPE(UString)

/*!
 * Some common and widely used types.
 * \file types.hpp
 */

/*!
 * \class UString
 * \brief A more C/C++ friendly string class.
 * Provides a simpler means to bridge the gap between QString's 32 bit
 * unicode, and simple utf8 strings that often are needed to directly
 * call C functions which use pure character functions.
 */

#endif