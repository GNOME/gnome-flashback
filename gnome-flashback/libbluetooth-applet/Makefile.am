NULL =

noinst_LTLIBRARIES = \
	libbluetooth-applet.la \
	$(NULL)

libbluetooth_applet_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"bluetooth-applet\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_builddir)/gnome-flashback/libbluetooth-applet \
	-I$(top_srcdir) \
	$(NULL)

libbluetooth_applet_la_CFLAGS = \
	$(BLUETOOTH_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libbluetooth_applet_la_SOURCES = \
  gf-bluetooth-applet.c \
	gf-bluetooth-applet.h \
	$(BUILT_SOURCES) \
	$(NULL)

libbluetooth_applet_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libbluetooth_applet_la_LIBADD = \
	$(top_builddir)/libstatus-notifier/libstatus-notifier.la \
	$(BLUETOOTH_APPLET_LIBS) \
	$(NULL)

gf-sd-rfkill.h:
gf-sd-rfkill.c: org.gnome.SettingsDaemon.Rfkill.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.SettingsDaemon.Rfkill. \
		--c-namespace Gf \
		--generate-c-code gf-sd-rfkill \
		$(srcdir)/org.gnome.SettingsDaemon.Rfkill.xml

BUILT_SOURCES = \
	gf-sd-rfkill.c \
	gf-sd-rfkill.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.SettingsDaemon.Rfkill.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
