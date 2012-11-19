# QtCreator project file

QT = core

CONFIG    = debug
CONFIG   += static
CONFIG   += link_pkgconfig qt warn_on

DEFINES   = DEBUG
DEFINES  += QTCREATOR_TEST

DEFINES  += WANT_ZYNADDSUBFX WANT_ZYNADDSUBFX_GUI

PKGCONFIG = fftw3 mxml ntk

TARGET   = carla_native
TEMPLATE = lib
VERSION  = 0.5.0

SOURCES = \
    bypass.c \
    midi-split.cpp \
    zynaddsubfx.cpp \
    zynaddsubfx-src.cpp

SOURCES += \
    3bandeq.cpp \
    3bandeq-src.cpp \
    3bandsplitter.cpp \
    distrho/pugl.cpp

HEADERS = \
    carla_native.h \
    carla_native.hpp

INCLUDEPATH = . distrho \
    ../carla-includes \
    ../carla-utils \
    ../distrho-plugin-toolkit

QMAKE_CFLAGS   *= -std=c99
QMAKE_CXXFLAGS *= -std=c++0x
