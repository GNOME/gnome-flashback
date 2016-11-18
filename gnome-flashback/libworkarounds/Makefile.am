NULL =

noinst_LTLIBRARIES = \
	libworkarounds.la \
	$(NULL)

libworkarounds_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"workarounds\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_builddir)/gnome-flashback/libworkarounds \
	$(NULL)

libworkarounds_la_CFLAGS = \
	$(WORKAROUNDS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libworkarounds_la_SOURCES = \
	gf-workarounds.c \
	gf-workarounds.h \
	$(NULL)

libworkarounds_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libworkarounds_la_LIBADD = \
	$(WORKAROUNDS_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
