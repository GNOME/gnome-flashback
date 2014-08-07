sessiondir       = $(datadir)/gnome-session/sessions
session_in_files = gnome-flashback.session.in
session_DATA     = $(session_in_files:.session.in=.session)

%.session: %.session.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

xsessiondir       = $(datadir)/xsessions
xsession_in_files = gnome-flashback.desktop.in
xsession_DATA     = $(xsession_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

EXTRA_DIST = \
	$(session_in_files) \
	$(xsession_in_files)

CLEANFILES = \
	$(session_DATA) \
	$(xsession_DATA)

-include $(top_srcdir)/git.mk
