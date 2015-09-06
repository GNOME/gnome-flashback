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

libsound_applet_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libsound_applet_la_LIBADD = \
	-lm \
	$(SOUND_APPLET_LIBS) \
	gvc/libgvc.la \
	$(NULL)

-include $(top_srcdir)/git.mk
