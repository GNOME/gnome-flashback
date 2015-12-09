NULL =

noinst_LTLIBRARIES = \
	libwatcher.la \
	$(NULL)

libwatcher_la_CPPFLAGS = \
	-I$(top_builddir)/gnome-flashback/libwatcher \
	$(NULL)

libwatcher_la_CFLAGS = \
	$(WATCHER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libwatcher_la_SOURCES = \
	gf-watcher.c \
	gf-watcher.h \
	$(NULL)

libwatcher_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libwatcher_la_LIBADD = \
	$(WATCHER_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
