NULL =

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

menudir   = $(sysconfdir)/xdg/menus
menu_DATA = gnome-flashback-applications.menu

sessiondir       = $(datadir)/gnome-session/sessions
session_in_files = gnome-flashback-compiz.session.in gnome-flashback-metacity.session.in
session_DATA     = $(session_in_files:.session.in=.session)

%.session: %.session.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t -d $(top_srcdir)/po && mv $@-t $@

autostartdir       = $(sysconfdir)/xdg/autostart
autostart_in_files = gnome-flashback-nm-applet.desktop.in gnome-flashback-screensaver.desktop.in
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

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t -d $(top_srcdir)/po && mv $@-t $@

gsettings_SCHEMAS = org.gnome.gnome-flashback.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(autostart_in_files) \
	$(desktop_in_files) \
	$(menu_DATA) \
	$(session_in_files) \
	$(xsession_in_in_files) \
	$(gsettings_SCHEMAS) \
	$(NULL)

CLEANFILES = \
	$(autostart_DATA) \
	$(desktop_DATA) \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	$(session_DATA) \
	$(xsession_in_files) \
	$(xsession_DATA) \
	*.gschema.valid \
	$(NULL)

-include $(top_srcdir)/git.mk
