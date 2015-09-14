NULL =

SUBDIRS = \
	applications \
	autostart \
	directories \
	menus \
	sessions \
	xsessions \
	$(NULL)

libexec_SCRIPTS = \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	$(NULL)

gnome-flashback-compiz: Makefile
	$(AM_V_GEN) (echo '#! /bin/sh' && echo 'exec gnome-session --session=gnome-flashback-compiz "$$@"') > $@
	$(AM_V_at) chmod a+x $@

gnome-flashback-metacity: Makefile
	$(AM_V_GEN) (echo '#! /bin/sh' && echo 'exec gnome-session --session=gnome-flashback-metacity --disable-acceleration-check "$$@"') > $@
	$(AM_V_at) chmod a+x $@

gsettings_SCHEMAS = org.gnome.gnome-flashback.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(gsettings_SCHEMAS) \
	$(NULL)

CLEANFILES = \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	*.gschema.valid \
	$(NULL)

-include $(top_srcdir)/git.mk
