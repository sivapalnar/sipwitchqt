# VoIP stack components that interface directly with eXosip

HEADERS += \
    $${PWD}/context.hpp \
    $${PWD}/stack.hpp \
    $${PWD}/contact.hpp \
    $${PWD}/registry.hpp \
    $${PWD}/provider.hpp \
    $${PWD}/call.hpp \
    $${PWD}/event.hpp \

SOURCES += \
    $${PWD}/context.cpp \
    $${PWD}/stack.cpp \
    $${PWD}/contact.cpp \
    $${PWD}/registry.cpp \
    $${PWD}/provider.cpp \
    $${PWD}/call.cpp \
    $${PWD}/event.cpp \

LIBS += -leXosip2 -losip2 -losipparser2

# required osx homebrew dependencies
macx {
    !exists(/usr/local/opt/libexosip):error(*** brew install libexosip)

    INCLUDEPATH += /usr/local/opt/libosip/include /usr/local/opt/libexosip/include
    LIBS += -L/usr/local/opt/libosip/lib -L/usr/local/opt/libexosip/lib
}