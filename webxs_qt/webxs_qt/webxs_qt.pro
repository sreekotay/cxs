TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
LIBS += -ldl -lpthread

SOURCES += \
    ../../webxs.c \
    ../../xs/miniz_116.c

HEADERS += \
    ../../xs/khash.h \
    ../../xs/xs_arr.h \
    ../../xs/xs_atomic.h \
    ../../xs/xs_compress.h \
    ../../xs/xs_connection.h \
    ../../xs/xs_crc.h \
    ../../xs/xs_fileinfo.h \
    ../../xs/xs_logger.h \
    ../../xs/xs_md5.h \
    ../../xs/xs_pipe.h \
    ../../xs/xs_posix_emu.h \
    ../../xs/xs_printf.h \
    ../../xs/xs_queue.h \
    ../../xs/xs_server.h \
    ../../xs/xs_sha1.h \
    ../../xs/xs_socket.h \
    ../../xs/xs_ssl.h \
    ../../xs/xs_types.h \
    ../../xs/xs_utils.h \
    ../../xs/xs_startup.h

