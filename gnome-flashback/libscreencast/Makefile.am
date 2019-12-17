NULL =

noinst_LTLIBRARIES = \
	libscreencast.la \
	$(NULL)

libscreencast_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"screencast\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libscreencast_la_CFLAGS = \
	$(SCREENCAST_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libscreencast_la_SOURCES = \
	gf-screencast.c \
	gf-screencast.h \
	$(NULL)

libscreencast_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreencast_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(SCREENCAST_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
