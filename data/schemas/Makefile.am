NULL =

gsettings_SCHEMAS = \
	org.gnome.gnome-flashback.gschema.xml \
	org.gnome.gnome-flashback.desktop-background.gschema.xml \
	org.gnome.gnome-flashback.input-sources.gschema.xml \
	org.gnome.gnome-flashback.workarounds.gschema.xml \
	$(NULL)

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(gsettings_SCHEMAS) \
	$(NULL)

CLEANFILES = \
	*.gschema.valid \
	$(NULL)

-include $(top_srcdir)/git.mk
