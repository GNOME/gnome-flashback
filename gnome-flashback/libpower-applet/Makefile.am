NULL =

noinst_LTLIBRARIES = \
	libpower-applet.la \
	$(NULL)

libpower_applet_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"power-applet\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_builddir)/gnome-flashback/libpower-applet \
	-I$(top_srcdir) \
	$(NULL)

libpower_applet_la_CFLAGS = \
	$(POWER_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libpower_applet_la_SOURCES = \
  gf-power-applet.c \
	gf-power-applet.h \
	$(BUILT_SOURCES) \
	$(NULL)

libpower_applet_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libpower_applet_la_LIBADD = \
	$(top_builddir)/libstatus-notifier/libstatus-notifier.la \
	$(POWER_APPLET_LIBS) \
	$(NULL)

gf-upower-device.h:
gf-upower-device.c: org.freedesktop.UPower.Device.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.freedesktop.UPower.Device. \
		--c-namespace Gf \
		--generate-c-code gf-upower-device \
		$(srcdir)/org.freedesktop.UPower.Device.xml

BUILT_SOURCES = \
	gf-upower-device.c \
	gf-upower-device.h \
	$(NULL)

EXTRA_DIST = \
	org.freedesktop.UPower.Device.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
