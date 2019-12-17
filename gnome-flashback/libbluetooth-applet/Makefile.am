NULL =

noinst_LTLIBRARIES = \
	libbluetooth-applet.la \
	$(NULL)

libbluetooth_applet_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"bluetooth-applet\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libbluetooth_applet_la_CFLAGS = \
	$(BLUETOOTH_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libbluetooth_applet_la_SOURCES = \
  gf-bluetooth-applet.c \
	gf-bluetooth-applet.h \
	$(NULL)

libbluetooth_applet_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libbluetooth_applet_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(BLUETOOTH_APPLET_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
