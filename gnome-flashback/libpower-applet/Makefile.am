NULL =

noinst_LTLIBRARIES = \
	libpower-applet.la \
	$(NULL)

libpower_applet_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"power-applet\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libpower_applet_la_CFLAGS = \
	$(POWER_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libpower_applet_la_SOURCES = \
  gf-power-applet.c \
	gf-power-applet.h \
	$(NULL)

libpower_applet_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libpower_applet_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(POWER_APPLET_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
