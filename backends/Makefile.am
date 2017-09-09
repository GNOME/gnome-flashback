NULL =

noinst_LTLIBRARIES = \
	libbackends.la \
	$(NULL)

libbackends_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gf-backends\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libbackends_la_CFLAGS = \
	$(BACKENDS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libbackends_la_SOURCES = \
	gf-backend-native-private.h \
	gf-backend-native.c \
	gf-backend-private.h \
	gf-backend-x11-cm-private.h \
	gf-backend-x11-cm.c \
	gf-backend-x11-nested-private.h \
	gf-backend-x11-nested.c \
	gf-backend-x11-private.h \
	gf-backend-x11.c \
	gf-backend.c \
	gf-backend.h \
	$(BUILT_SOURCES) \
	$(NULL)

libbackends_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libbackends_la_LIBADD = \
	$(BACKENDS_LIBS) \
	$(LIBM) \
	$(NULL)

BUILT_SOURCES = \
	$(NULL)

EXTRA_DIST = \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
