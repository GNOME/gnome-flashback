libexec_SCRIPTS = \
	gnome-flashback-compiz \
	gnome-flashback-metacity

gnome-flashback-compiz: Makefile
	$(AM_V_GEN) (echo '#! /bin/sh' && echo 'exec gnome-session --session=gnome-flashback-compiz "$$@"') > $@
	$(AM_V_at) chmod a+x $@

gnome-flashback-metacity: Makefile
	$(AM_V_GEN) (echo '#! /bin/sh' && echo 'exec gnome-session --session=gnome-flashback-metacity "$$@"') > $@
	$(AM_V_at) chmod a+x $@

sessiondir       = $(datadir)/gnome-session/sessions
session_in_files = gnome-flashback-compiz.session.in gnome-flashback-metacity.session.in
session_DATA     = $(session_in_files:.session.in=.session)

%.session: %.session.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

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

EXTRA_DIST = \
	$(desktop_in_files) \
	$(session_in_files) \
	$(xsession_in_in_files)

CLEANFILES = \
	$(desktop_DATA) \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	$(session_DATA) \
	$(xsession_in_files) \
	$(xsession_DATA)

-include $(top_srcdir)/git.mk
