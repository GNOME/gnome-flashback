noinst_LTLIBRARIES = \
	libdisplay-config.la

AM_CPPFLAGS = \
	$(DISPLAY_CONFIG_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libdisplay-config \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

libdisplay_config_la_SOURCES = \
	edid-parse.c \
	edid.h \
	meta-dbus-display-config.c \
	meta-dbus-display-config.h \
	meta-display-config-shared.h \
	flashback-display-config.c \
	flashback-display-config.h \
	flashback-monitor-config.c \
	flashback-monitor-config.h \
	flashback-monitor-manager.c \
	flashback-monitor-manager.h

libdisplay_config_la_LIBADD = \
	$(DISPLAY_CONFIG_LIBS)

meta-dbus-display-config.h:
meta-dbus-display-config.c: org.gnome.Mutter.DisplayConfig.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace MetaDBus \
		--generate-c-code meta-dbus-display-config \
		$(srcdir)/org.gnome.Mutter.DisplayConfig.xml

BUILT_SOURCES = \
	meta-dbus-display-config.h \
	meta-dbus-display-config.c

EXTRA_DIST = \
	org.gnome.Mutter.DisplayConfig.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
