libexec_SCRIPTS = \
	gnome-flashback-compiz \
	gnome-flashback-metacity

gnome-flashback-compiz: Makefile
	$(AM_V_GEN) (echo '#! /bin/sh' && echo 'exec gnome-session --session=gnome-flashback-compiz "$$@"') > $@
	$(AM_V_at) chmod a+x $@

gnome-flashback-metacity: Makefile
	$(AM_V_GEN) (echo '#! /bin/sh' && echo 'exec gnome-session --session=gnome-flashback-metacity --disable-acceleration-check "$$@"') > $@
	$(AM_V_at) chmod a+x $@

menudir   = $(sysconfdir)/xdg/menus
menu_DATA = gnome-flashback-applications.menu

sessiondir       = $(datadir)/gnome-session/sessions
session_in_files = gnome-flashback-compiz.session.in gnome-flashback-metacity.session.in
session_DATA     = $(session_in_files:.session.in=.session)

%.session: %.session.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

autostartdir       = $(sysconfdir)/xdg/autostart
autostart_in_files = gnome-flashback-nm-applet.desktop.in
autostart_DATA     = $(autostart_in_files:.desktop.in=.desktop)

desktopdir       = $(datadir)/applications
desktop_in_files = gnome-flashback.desktop.in gnome-flashback-init.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)

xsessiondir          = $(datadir)/xsessions
xsession_in_in_files = gnome-flashback-compiz.desktop.in.in gnome-flashback-metacity.desktop.in.in
xsession_in_files    = gnome-flashback-compiz.desktop.in gnome-flashback-metacity.desktop.in
xsession_DATA        = $(xsession_in_files:.desktop.in=.desktop)

%.desktop.in: %.desktop.in.in
	$(AM_V_GEN) sed -e 's,[@]libexecdir[@],$(libexecdir),g' < $< > $@

%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

@INTLTOOL_XML_NOMERGE_RULE@

gsettings_schemas_in_in = org.gnome.gnome-flashback.gschema.xml.in.in
gsettings_schemas_in    = $(gsettings_schemas_in_in:.xml.in.in=.xml.in)
gsettings_SCHEMAS       = $(gsettings_schemas_in:.xml.in=.xml)

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(autostart_in_files) \
	$(desktop_in_files) \
	$(gsettings_schemas_in_in) \
	$(menu_DATA) \
	$(session_in_files) \
	$(xsession_in_in_files)

CLEANFILES = \
	$(autostart_DATA) \
	$(desktop_DATA) \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	$(gsettings_schemas_in) \
	$(gsettings_SCHEMAS) \
	*.gschema.valid \
	$(session_DATA) \
	$(xsession_in_files) \
	$(xsession_DATA)

-include $(top_srcdir)/git.mk
