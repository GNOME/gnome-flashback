NULL =

SUBDIRS = \
	liba11y-keyboard \
	libaudio-device-selection \
	libautomount-manager \
	libcommon \
	libdesktop \
	libend-session-dialog \
	libinput-settings \
	libinput-sources \
	libnotifications \
	libroot-background \
	libscreencast \
	libscreensaver \
	libscreenshot \
	libshell \
	libstatus-notifier-watcher \
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
	gf-ui-scaling.c \
	gf-ui-scaling.h \
	gf-wm.c \
	gf-wm.h \
	$(BUILT_SOURCES) \
	$(NULL)

gnome_flashback_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS) \
	$(top_builddir)/backends/libbackends.la \
	$(top_builddir)/dbus/libdbus.la \
	$(top_builddir)/gnome-flashback/liba11y-keyboard/liba11y-keyboard.la \
	$(top_builddir)/gnome-flashback/libaudio-device-selection/libaudio-device-selection.la \
	$(top_builddir)/gnome-flashback/libautomount-manager/libautomount-manager.la \
	$(top_builddir)/gnome-flashback/libdesktop/libdesktop.la \
	$(top_builddir)/gnome-flashback/libend-session-dialog/libend-session-dialog.la \
	$(top_builddir)/gnome-flashback/libinput-settings/libinput-settings.la \
	$(top_builddir)/gnome-flashback/libinput-sources/libinput-sources.la \
	$(top_builddir)/gnome-flashback/libnotifications/libnotifications.la \
	$(top_builddir)/gnome-flashback/libroot-background/libroot-background.la \
	$(top_builddir)/gnome-flashback/libscreencast/libscreencast.la \
	$(top_builddir)/gnome-flashback/libscreensaver/libscreensaver.la \
	$(top_builddir)/gnome-flashback/libscreenshot/libscreenshot.la \
	$(top_builddir)/gnome-flashback/libshell/libshell.la \
	$(top_builddir)/gnome-flashback/libstatus-notifier-watcher/libstatus-notifier-watcher.la \
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
