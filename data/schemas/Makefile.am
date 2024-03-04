NULL =

overridedir = $(gsettingsschemadir)
override_DATA = 00_gnome-flashback.gschema.override

gsettings_ENUM_NAMESPACE = org.gnome.gnome-flashback.desktop
gsettings_ENUM_FILES = \
	$(top_srcdir)/gnome-flashback/libdesktop/gf-desktop-enums.h \
	$(NULL)

gsettings_SCHEMAS = \
	org.gnome.gnome-flashback.gschema.xml \
	org.gnome.gnome-flashback.desktop.gschema.xml \
	org.gnome.gnome-flashback.desktop.background.gschema.xml \
	org.gnome.gnome-flashback.desktop.icons.gschema.xml \
	org.gnome.gnome-flashback.keybindings.gschema.xml \
	org.gnome.gnome-flashback.notifications.gschema.xml \
	org.gnome.gnome-flashback.system-indicators.input-sources.gschema.xml \
	$(NULL)

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(override_DATA) \
	$(gsettings_SCHEMAS) \
	$(NULL)

CLEANFILES = \
	*.gschema.valid \
	$(NULL)

-include $(top_srcdir)/git.mk
