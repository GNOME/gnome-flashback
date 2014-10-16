SUBDIRS = \
	libdesktop-background \
	libdisplay-config \
	libend-session-dialog \
	libidle-monitor \
	libsound-applet

bin_PROGRAMS = \
	gnome-flashback

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	-I$(top_builddir)/gnome-flashback \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

gnome_flashback_SOURCES = \
	flashback-application.c \
	flashback-application.h \
	flashback-main.c

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS) \
	$(top_builddir)/gnome-flashback/libdesktop-background/libdesktop-background.la \
	$(top_builddir)/gnome-flashback/libdisplay-config/libdisplay-config.la \
	$(top_builddir)/gnome-flashback/libend-session-dialog/libend-session-dialog.la \
	$(top_builddir)/gnome-flashback/libidle-monitor/libidle-monitor.la \
	$(top_builddir)/gnome-flashback/libsound-applet/libsound-applet.la

@INTLTOOL_XML_NOMERGE_RULE@

gsettings_schemas_in_in = org.gnome.gnome-flashback.gschema.xml.in.in
gsettings_schemas_in    = $(gsettings_schemas_in_in:.xml.in.in=.xml.in)
gsettings_SCHEMAS       = $(gsettings_schemas_in:.xml.in=.xml)

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(gsettings_schemas_in_in)

CLEANFILES = \
	$(gsettings_schemas_in) \
	$(gsettings_SCHEMAS) \
	*.gschema.valid

-include $(top_srcdir)/git.mk
