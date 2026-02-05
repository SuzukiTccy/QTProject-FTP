QT       += core gui network concurrent

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ftp.cpp \
    ftpconnectdlg.cpp \
    ftplib.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    ftp.h   \
    ftpconnectdlg.h \
    ftplib.h \
    mainwindow.h

FORMS += \
    ftpconnectdlg.ui \
    mainwindow.ui

TRANSLATIONS += \
    FTPTransFer-client_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

QMAKE_MACOSX_DEPLOYMENT_TARGET = 26.0

# INCLUDEPATH += $$PWD/ftplib/include
# DEPENDPATH += $$PWD/ftplib/include
# LIBS += -L$$PWD/ftplib/lib/ -lftplib
# LIBS += -L$$PWD/ftplib/lib/ -lftplibd


INCLUDEPATH += /opt/homebrew/opt/openssl/include
DEPENDPATH += /opt/homebrew/opt/openssl/include
LIBS += -L/opt/homebrew/opt/openssl/lib -lssl -lcrypto

# PRE_TARGETDEPS += $$PWD/ftplib/lib/libftplib.a
# PRE_TARGETDEPS += $$PWD/ftplib/lib/libftplibd.a
# PRE_TARGETDEPS += $$PWD/ftplib/lib/ftplib.lib
# PRE_TARGETDEPS += $$PWD/ftplib/lib/ftplibd.lib
