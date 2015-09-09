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
	$(BUILT_SOURCES) \
	$(NULL)

libbluetooth_applet_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libbluetooth_applet_la_LIBADD = \
	$(BLUETOOTH_APPLET_LIBS) \
	$(NULL)

gf-settings-daemon-rfkill.h:
gf-settings-daemon-rfkill.c: org.gnome.SettingsDaemon.Rfkill.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.SettingsDaemon.Rfkill. \
		--c-namespace Gf \
		--generate-c-code gf-settings-daemon-rfkill \
		$(srcdir)/org.gnome.SettingsDaemon.Rfkill.xml

BUILT_SOURCES = \
	gf-settings-daemon-rfkill.c \
	gf-settings-daemon-rfkill.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.SettingsDaemon.Rfkill.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
