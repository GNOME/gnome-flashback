NULL =

SUBDIRS = \
	gvc \
	$(NULL)

noinst_LTLIBRARIES = \
	libsound-applet.la \
	$(NULL)

libsound_applet_la_CFLAGS = \
	$(SOUND_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libsound-applet \
	-I$(srcdir)/gvc \
	$(NULL)

libsound_applet_la_SOURCES = \
	gf-sound-applet.c \
	gf-sound-applet.h \
	gvc-channel-bar.c \
	gvc-channel-bar.h \
	gvc-stream-status-icon.c \
	gvc-stream-status-icon.h \
	$(NULL)

if WITH_LIBSTATUS_NOTIFIER
libsound_applet_la_SOURCES += \
	gf-sound-item.c \
	gf-sound-item.h \
	$(NULL)
endif

libsound_applet_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libsound_applet_la_LIBADD = \
	gvc/libgvc.la \
	$(SOUND_APPLET_LIBS) \
	$(LIBM) \
	$(NULL)

-include $(top_srcdir)/git.mk
