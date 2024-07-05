# Copyright 2021 CyberTech Labs Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

QT       += core gui widgets concurrent

QM_FILES_RESOURCE_PREFIX = /translations

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    $$PWD/checker.cpp \
    $$PWD/main.cpp \
    $$PWD/mainwindow.cpp \

HEADERS += \
    $$PWD/checker.h \
    $$PWD/htmlTemplates.h \
    $$PWD/mainwindow.h \
    $$PWD/optionsAliases.h \

FORMS += \
    $$PWD/mainwindow.ui

TRANSLATIONS += \
    $$PWD/translations/checkapp_ru.ts

unix:target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32 {
        QMAKE_MANIFEST = $$PWD/application.exe.manifest
        RC_ICONS = $$PWD/icon.ico
        DISTFILES += $$QMAKE_MANIFEST $$RC_ICONS
}

macx {
        ICON = $$PWD/icon.icns
}

RESOURCES += \
    $$PWD/resources.qrc

DISTFILES += \
    checkapp.pri
s
