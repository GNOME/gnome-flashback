NULL =

noinst_LTLIBRARIES = \
	libworkarounds.la \
	$(NULL)

AM_CPPFLAGS = \
	$(IDLE_MONITOR_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libworkarounds \
	$(NULL)

libworkarounds_la_SOURCES = \
	flashback-workarounds.c \
	flashback-workarounds.h \
	$(NULL)

libworkarounds_la_LIBADD = \
	$(IDLE_MONITOR_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
