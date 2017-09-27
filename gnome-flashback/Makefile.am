NULL =

SUBDIRS = \
	libaudio-device-selection \
	libautomount-manager \
	libbluetooth-applet \
	libcommon \
	libdesktop-background \
	libdisplay-config \
	libend-session-dialog \
	libidle-monitor \
	libinput-settings \
	libinput-sources \
	libnotifications \
	libpolkit \
	libpower-applet \
	libscreencast \
	libscreensaver \
	libscreenshot \
	libshell \
	libsound-applet \
	libstatus-notifier-watcher \
	libworkarounds \
	$(NULL)

bin_PROGRAMS = \
	gnome-flashback \
	$(NULL)

gnome_flashback_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gnome-flashback\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DLOCALE_DIR=\""$(localedir)"\" \
	-I$(top_builddir)/gnome-flashback \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

gnome_flashback_CFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

gnome_flashback_SOURCES = \
	gf-application.c \
	gf-application.h \
	gf-confirm-display-change-dialog.c \
	gf-confirm-display-change-dialog.h \
	gf-main.c \
	gf-session.c \
	gf-session.h \
	$(BUILT_SOURCES) \
	$(NULL)

gnome_flashback_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS) \
	$(top_builddir)/backends/libbackends.la \
	$(top_builddir)/gnome-flashback/libaudio-device-selection/libaudio-device-selection.la \
	$(top_builddir)/gnome-flashback/libautomount-manager/libautomount-manager.la \
	$(top_builddir)/gnome-flashback/libbluetooth-applet/libbluetooth-applet.la \
	$(top_builddir)/gnome-flashback/libdesktop-background/libdesktop-background.la \
	$(top_builddir)/gnome-flashback/libdisplay-config/libdisplay-config.la \
	$(top_builddir)/gnome-flashback/libend-session-dialog/libend-session-dialog.la \
	$(top_builddir)/gnome-flashback/libidle-monitor/libidle-monitor.la \
	$(top_builddir)/gnome-flashback/libinput-settings/libinput-settings.la \
	$(top_builddir)/gnome-flashback/libinput-sources/libinput-sources.la \
	$(top_builddir)/gnome-flashback/libnotifications/libnotifications.la \
	$(top_builddir)/gnome-flashback/libpolkit/libpolkit.la \
	$(top_builddir)/gnome-flashback/libpower-applet/libpower-applet.la \
	$(top_builddir)/gnome-flashback/libscreencast/libscreencast.la \
	$(top_builddir)/gnome-flashback/libscreensaver/libscreensaver.la \
	$(top_builddir)/gnome-flashback/libscreenshot/libscreenshot.la \
	$(top_builddir)/gnome-flashback/libshell/libshell.la \
	$(top_builddir)/gnome-flashback/libsound-applet/libsound-applet.la \
	$(top_builddir)/gnome-flashback/libstatus-notifier-watcher/libstatus-notifier-watcher.la \
	$(top_builddir)/gnome-flashback/libworkarounds/libworkarounds.la \
	$(NULL)

gf-resources.c: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name gf $<

gf-resources.h: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name gf $<

BUILT_SOURCES = \
	gf-resources.c \
	gf-resources.h \
	$(NULL)

EXTRA_DIST = \
	flashback.gresource.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
