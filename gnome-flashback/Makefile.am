SUBDIRS = \
	libdesktop-background \
	libend-session-dialog

bin_PROGRAMS = \
	gnome-flashback

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	-I$(top_builddir)/gnome-flashback \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

gnome_flashback_SOURCES = \
	dbus-display-config.c \
	dbus-display-config.h \
	dbus-idle-monitor.c \
	dbus-idle-monitor.h \
	flashback-application.c \
	flashback-application.h \
	flashback-display-config.c \
	flashback-display-config.h \
	flashback-idle-monitor.c \
	flashback-idle-monitor.h \
	flashback-main.c

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS) \
	$(top_builddir)/gnome-flashback/libdesktop-background/libdesktop-background.la \
	$(top_builddir)/gnome-flashback/libend-session-dialog/libend-session-dialog.la

desktopdir       = $(datadir)/applications
desktop_in_files = gnome-flashback.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

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

BUILT_SOURCES = \
	dbus-display-config.h \
	dbus-display-config.c \
	dbus-idle-monitor.h \
	dbus-idle-monitor.c

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
	org.gnome.Mutter.DisplayConfig.xml \
	org.gnome.Mutter.IdleMonitor.xml

CLEANFILES = \
	$(desktop_DATA) \
	$(BUILT_SOURCES) \
	$(gsettings_schemas_in) \
	$(gsettings_SCHEMAS) \
	*.gschema.valid

-include $(top_srcdir)/git.mk
