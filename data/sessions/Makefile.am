NULL =

required_components_common = \
	gnome-flashback \
	gnome-panel \
	$(NULL)

required_components_compiz = \
	compiz \
	$(required_components_common) \
	$(REQUIRED_COMPNENTS_GSD) \
	$(NULL)

required_components_metacity = \
	metacity \
	$(required_components_common) \
	$(REQUIRED_COMPNENTS_GSD) \
	$(NULL)

space = $(NULL) $(NULL)
compiz = $(subst $(space),;,$(strip $(required_components_compiz)))
metacity = $(subst $(space),;,$(strip $(required_components_metacity)))

gnome-flashback-metacity.session: ../../configure.ac Makefile.am gnome-flashback.session.in
	$(AM_V_GEN) sed \
		-e 's,[@]NAME[@],Metacity,g' \
		-e 's,[@]REQUIRED_COMPONENTS[@],$(metacity),g' \
		< $(srcdir)/gnome-flashback.session.in > $@

sessions = \
	gnome-flashback-metacity.session \
	$(NULL)

if WITH_COMPIZ_SESSION
gnome-flashback-compiz.session: ../../configure.ac Makefile.am gnome-flashback.session.in
	$(AM_V_GEN) sed \
		-e 's,[@]NAME[@],Compiz,g' \
		-e 's,[@]REQUIRED_COMPONENTS[@],$(compiz),g' \
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
