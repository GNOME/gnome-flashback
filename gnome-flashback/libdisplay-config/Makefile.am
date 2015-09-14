NULL =

noinst_LTLIBRARIES = \
	libdisplay-config.la
	$(NULL)

libdisplay_config_la_CFLAGS = \
	$(DISPLAY_CONFIG_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libdisplay-config \
	$(NULL)

libdisplay_config_la_SOURCES = \
	edid-parse.c \
	edid.h \
	meta-dbus-display-config.c \
	meta-dbus-display-config.h \
	meta-display-config-shared.h \
	flashback-confirm-dialog.c \
	flashback-confirm-dialog.h \
	flashback-display-config.c \
	flashback-display-config.h \
	flashback-monitor-config.c \
	flashback-monitor-config.h \
	flashback-monitor-manager.c \
	flashback-monitor-manager.h \
	$(NULL)

libdisplay_config_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdisplay_config_la_LIBADD = \
	$(DISPLAY_CONFIG_LIBS) \
	$(NULL)

meta-dbus-display-config.h:
meta-dbus-display-config.c: org.gnome.Mutter.DisplayConfig.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace MetaDBus \
		--generate-c-code meta-dbus-display-config \
		$(srcdir)/org.gnome.Mutter.DisplayConfig.xml

BUILT_SOURCES = \
	meta-dbus-display-config.h \
	meta-dbus-display-config.c \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Mutter.DisplayConfig.xml \
	flashback-confirm-dialog.ui \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
