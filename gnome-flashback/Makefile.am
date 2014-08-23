bin_PROGRAMS = \
	gnome-flashback

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	-I$(top_builddir)/gnome-flashback \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DGNOME_DESKTOP_USE_UNSTABLE_API

gnome_flashback_SOURCES = \
	flashback-application.c \
	flashback-application.h \
	flashback-dbus-display-config.c \
	flashback-dbus-display-config.h \
	flashback-dbus-idle-monitor.c \
	flashback-dbus-idle-monitor.h \
	flashback-desktop-background.c \
	flashback-desktop-background.h \
	flashback-end-session-dialog.c \
	flashback-end-session-dialog.h \
	flashback-gsettings.h \
	flashback-inhibit-dialog.c \
	flashback-inhibit-dialog.h \
	flashback-main.c \
	flashback-resources.c \
	flashback-resources.h

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS)

desktopdir       = $(datadir)/applications
desktop_in_files = gnome-flashback.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

flashback-end-session-dialog.h:
flashback-end-session-dialog.c: org.gnome.SessionManager.EndSessionDialog.xml Makefile.am
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix=org.gnome.SessionManager \
		--generate-c-code flashback-end-session-dialog \
		--c-namespace Flashback \
		org.gnome.SessionManager.EndSessionDialog.xml

flashback-dbus-display-config.h:
flashback-dbus-display-config.c: org.gnome.Mutter.DisplayConfig.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace Flashback \
		--generate-c-code flashback-dbus-display-config \
		$(srcdir)/org.gnome.Mutter.DisplayConfig.xml

flashback-dbus-idle-monitor.h:
flashback-dbus-idle-monitor.c: org.gnome.Mutter.IdleMonitor.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace Flashback \
		--generate-c-code flashback-dbus-idle-monitor \
		--c-generate-object-manager \
		$(srcdir)/org.gnome.Mutter.IdleMonitor.xml

flashback-resources.c: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<
flashback-resources.h: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<

BUILT_SOURCES = \
	flashback-end-session-dialog.h \
	flashback-end-session-dialog.c \
	flashback-dbus-display-config.h \
	flashback-dbus-display-config.c \
	flashback-dbus-idle-monitor.h \
	flashback-dbus-idle-monitor.c \
	flashback-resources.c \
	flashback-resources.h

EXTRA_DIST = \
	$(desktop_in_files) \
	org.gnome.SessionManager.EndSessionDialog.xml \
	org.gnome.Mutter.DisplayConfig.xml \
	org.gnome.Mutter.IdleMonitor.xml \
	flashback.gresource.xml \
	flashback-inhibit-dialog.ui

CLEANFILES = \
	$(desktop_DATA) \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
