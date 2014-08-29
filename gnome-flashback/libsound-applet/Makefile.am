SUBDIRS = \
	gvc

noinst_LTLIBRARIES = \
	libsound-applet.la

AM_CPPFLAGS = \
	$(SOUND_APPLET_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libsound-applet \
	-I$(srcdir)/gvc

libsound_applet_la_SOURCES = \
	gvc-applet.c \
	gvc-applet.h \
	gvc-channel-bar.c \
	gvc-channel-bar.h \
	gvc-stream-status-icon.c \
	gvc-stream-status-icon.h

libsound_applet_la_LIBADD = \
	-lm \
	$(SOUND_APPLET_LIBS) \
	gvc/libgvc.la

-include $(top_srcdir)/git.mk
