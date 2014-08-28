bin_PROGRAMS = \
	gnome-flashback

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	-I$(top_builddir)/gnome-flashback \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DGNOME_DESKTOP_USE_UNSTABLE_API

gnome_flashback_SOURCES = \
	dbus-display-config.c \
	dbus-display-config.h \
	dbus-end-session-dialog.c \
	dbus-end-session-dialog.h \
	dbus-idle-monitor.c \
	dbus-idle-monitor.h \
	flashback-application.c \
	flashback-application.h \
	flashback-desktop-background.c \
	flashback-desktop-background.h \
	flashback-display-config.c \
	flashback-display-config.h \
	flashback-end-session-dialog.c \
	flashback-end-session-dialog.h \
	flashback-gsettings.h \
	flashback-idle-monitor.c \
	flashback-idle-monitor.h \
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

dbus-end-session-dialog.h:
dbus-end-session-dialog.c: org.gnome.SessionManager.EndSessionDialog.xml Makefile.am
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix=org.gnome.SessionManager \
		--generate-c-code dbus-end-session-dialog \
		--c-namespace DBus \
		org.gnome.SessionManager.EndSessionDialog.xml

dbus-display-config.h:
dbus-display-config.c: org.gnome.Mutter.DisplayConfig.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace DBus \
		--generate-c-code dbus-display-config \
		$(srcdir)/org.gnome.Mutter.DisplayConfig.xml

dbus-idle-monitor.h:
dbus-idle-monitor.c: org.gnome.Mutter.IdleMonitor.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace DBus \
		--generate-c-code dbus-idle-monitor \
		--c-generate-object-manager \
		$(srcdir)/org.gnome.Mutter.IdleMonitor.xml

flashback-resources.c: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<
flashback-resources.h: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<

BUILT_SOURCES = \
	dbus-end-session-dialog.h \
	dbus-end-session-dialog.c \
	dbus-display-config.h \
	dbus-display-config.c \
	dbus-idle-monitor.h \
	dbus-idle-monitor.c \
	flashback-resources.c \
	flashback-resources.h

@INTLTOOL_XML_NOMERGE_RULE@

gsettings_schemas_in_in = org.gnome.gnome-flashback.gschema.xml.in.in
gsettings_schemas_in    = $(gsettings_schemas_in_in:.xml.in.in=.xml.in)
gsettings_SCHEMAS       = $(gsettings_schemas_in:.xml.in=.xml)

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(desktop_in_files) \
	$(gsettings_schemas_in_in) \
	org.gnome.SessionManager.EndSessionDialog.xml \
	org.gnome.Mutter.DisplayConfig.xml \
	org.gnome.Mutter.IdleMonitor.xml \
	flashback.gresource.xml \
	flashback-inhibit-dialog.ui

CLEANFILES = \
	$(desktop_DATA) \
	$(BUILT_SOURCES) \
	$(gsettings_schemas_in) \
	$(gsettings_SCHEMAS) \
	*.gschema.valid

-include $(top_srcdir)/git.mk
