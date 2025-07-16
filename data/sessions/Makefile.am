NULL =

gnome-flashback-metacity.session: Makefile.am gnome-flashback.session.in
	$(AM_V_GEN) sed \
		-e 's,[@]NAME[@],Metacity,g' \
		< $(srcdir)/gnome-flashback.session.in > $@

sessions = \
	gnome-flashback-metacity.session \
	$(NULL)

if WITH_COMPIZ_SESSION
gnome-flashback-compiz.session: Makefile.am gnome-flashback.session.in
	$(AM_V_GEN) sed \
		-e 's,[@]NAME[@],Compiz,g' \
		< $(srcdir)/gnome-flashback.session.in > $@

sessions += \
	gnome-flashback-compiz.session \
	$(NULL)
endif

sessiondir = $(datadir)/gnome-session/sessions
session_DATA = $(sessions)

CLEANFILES = \
	$(sessions) \
	$(NULL)

EXTRA_DIST = \
	gnome-flashback.session.in \
	$(NULL)

-include $(top_srcdir)/git.mk
