NULL =

noinst_LTLIBRARIES = \
	libscreenshot.la \
	$(NULL)

libscreenshot_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"screenshot\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libscreenshot_la_CFLAGS = \
	$(SCREENSHOT_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libscreenshot_la_SOURCES = \
	gf-flashspot.c \
	gf-flashspot.h \
	gf-screenshot.c \
	gf-screenshot.h \
	gf-select-area.c \
	gf-select-area.h \
	$(NULL)

libscreenshot_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreenshot_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(SCREENSHOT_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
