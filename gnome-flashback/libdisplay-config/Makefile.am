noinst_LTLIBRARIES = \
	libdisplay-config.la

AM_CPPFLAGS = \
	$(DISPLAY_CONFIG_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libdisplay-config \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

libdisplay_config_la_SOURCES = \
	dbus-display-config.c \
	dbus-display-config.h \
	flashback-display-config.c \
	flashback-display-config.h

libdisplay_config_la_LIBADD = \
	$(DISPLAY_CONFIG_LIBS)

dbus-display-config.h:
dbus-display-config.c: org.gnome.Mutter.DisplayConfig.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace DBus \
		--generate-c-code dbus-display-config \
		$(srcdir)/org.gnome.Mutter.DisplayConfig.xml

BUILT_SOURCES = \
	dbus-display-config.h \
	dbus-display-config.c

EXTRA_DIST = \
	org.gnome.Mutter.DisplayConfig.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
