NULL =

noinst_LTLIBRARIES = \
	libbluetooth-applet.la \
	$(NULL)

libbluetooth_applet_la_CFLAGS = \
	$(BLUETOOTH_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libbluetooth-applet \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
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
	$(BLUETOOTH_APPLET_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
