NULL =

noinst_LTLIBRARIES = \
	libsound-applet.la \
	$(NULL)

libsound_applet_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"sound-applet\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir)/gvc/gvc \
	$(NULL)

libsound_applet_la_CFLAGS = \
	$(SOUND_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
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
	$(top_builddir)/gvc/libgvc.la \
	$(SOUND_APPLET_LIBS) \
	$(LIBM) \
	$(NULL)

-include $(top_srcdir)/git.mk
