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
	$(NULL)

if WITH_SYSTEM_INDICATORS
gsettings_SCHEMAS += \
	org.gnome.gnome-flashback.system-indicators.input-source.gschema.xml \
	$(NULL)
endif

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(override_DATA) \
	$(gsettings_SCHEMAS) \
	$(NULL)

if !WITH_SYSTEM_INDICATORS
EXTRA_DIST += \
	org.gnome.gnome-flashback.system-indicators.input-source.gschema.xml \
	$(NULL)
endif

CLEANFILES = \
	*.gschema.valid \
	$(NULL)

-include $(top_srcdir)/git.mk
