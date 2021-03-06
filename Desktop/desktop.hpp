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

#ifndef DESKTOP_HPP_
#define	DESKTOP_HPP_

#include "../Common/compiler.hpp"
#include "../Connect/control.hpp"
#include "../Connect/listener.hpp"
#include "../Connect/storage.hpp"
#include "toolbar.hpp"
#include "statusbar.hpp"

#include "login.hpp"
#include "sessions.hpp"
#include "phonebook.hpp"
#include "options.hpp"

#include <QMainWindow>
#include <QString>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QSystemTrayIcon>
#include <QSettings>

#if defined(DESKTOP_PREFIX)
#define CONFIG_FROM DESKTOP_PREFIX "/settings.cfg", QSettings::IniFormat
#elif defined(Q_OS_MAC)
#define CONFIG_FROM "tychosoft.com", "Antisipate"
#elif defined(Q_OS_WIN)
#define CONFIG_FROM "Tycho Softworks", "Antisipate"
#else
#define CONFIG_FROM "tychosoft.com", "antisipate"
#endif

class Desktop final : public QMainWindow
{
	Q_OBJECT
    Q_DISABLE_COPY(Desktop)

public:
    typedef enum {
        INITIAL,
        OFFLINE,
        AUTHORIZING,
        CALLING,
        DISCONNECTING,
        ONLINE,
    } state_t;

    Desktop(bool tray, bool reset);
    virtual ~Desktop();

    bool isConnected() const {
        return connected;
    }

    bool isActive() const {
        return listener != nullptr;
    }

    bool isOpened() const {
        return storage != nullptr;
    }

    bool isLogin() const {
        return isCurrent(login);
    }

    bool isSession() const {
        return isCurrent(sessions);
    }

    bool isProfile() const {
        return isCurrent(phonebook);
    }

    bool isOptions() const {
        return isCurrent(options);
    }

    // status bar functions...
    void warning(const QString& msg);
    void error(const QString& msg);
    void status(const QString& msg);
    void clear();

    bool notify(const QString& title, const QString& body, QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information, int timeout = 10000);

    QWidget *extendToolbar(QToolBar *bar, QMenuBar *menu = nullptr);

    inline static Desktop *instance() {
        return Instance;
    }

    inline static state_t state() {
        return State;
    }

    inline static const QVariantHash credentials() {
        return Credentials;
    }

private:
    Login *login;
    Sessions *sessions;
    Phonebook *phonebook;
    Options *options;
    Control *control;
    Listener *listener;
    Storage *storage;
    Toolbar *toolbar;
    Statusbar *statusbar;
    QSettings settings;
    QSystemTrayIcon *trayIcon;
    QMenu *trayMenu, *dockMenu, *appMenu;
    bool restart_flag, connected;
    QVariantHash currentCredentials;
    QString appearance;

    void closeEvent(QCloseEvent *event) final;

    bool isCurrent(const QWidget *widget) const;
    void listen(const QVariantHash &cred);
    void setState(state_t state);

    static QVariantHash Credentials;   // current credentials...
    static Desktop *Instance;
    static state_t State;

signals:
    void online(bool state);

public slots:
    void initial(void);
    void dock_clicked();

private slots:
    void authorized(const QVariantHash& creds); // server authorized
    void offline(void);                         // lost server connection
    void authorizing(void);                     // registering with server...
    void failed(int error_code);                // sip session fatal error
    void shutdown();                            // application shutdown

    void appState(Qt::ApplicationState state);
    void trayAction(QSystemTrayIcon::ActivationReason reason);
    void trayAway();

    void showOptions(void);
    void showSessions(void);
    void showPhonebook(void);
};

#endif
