NULL =

required_components_common = \
	gnome-flashback-init \
	gnome-flashback \
	gnome-panel \
	$(NULL)

required_components_gsd = \
	org.gnome.SettingsDaemon.A11ySettings \
	org.gnome.SettingsDaemon.Color \
	org.gnome.SettingsDaemon.Datetime \
	org.gnome.SettingsDaemon.Housekeeping \
	org.gnome.SettingsDaemon.Keyboard \
	org.gnome.SettingsDaemon.MediaKeys \
	org.gnome.SettingsDaemon.Mouse \
	org.gnome.SettingsDaemon.Power \
	org.gnome.SettingsDaemon.PrintNotifications \
	org.gnome.SettingsDaemon.Rfkill \
	org.gnome.SettingsDaemon.ScreensaverProxy \
	org.gnome.SettingsDaemon.Sharing \
	org.gnome.SettingsDaemon.Smartcard \
	org.gnome.SettingsDaemon.Sound \
	org.gnome.SettingsDaemon.Wacom \
	org.gnome.SettingsDaemon.XSettings \
	$(NULL)

required_components_compiz = \
	compiz \
	$(required_components_common) \
	$(required_components_gsd) \
	$(NULL)

required_components_metacity = \
	metacity \
	$(required_components_common) \
	$(required_components_gsd) \
	$(NULL)

space = $(NULL) $(NULL)
compiz = $(subst $(space),;,$(strip $(required_components_compiz)))
metacity = $(subst $(space),;,$(strip $(required_components_metacity)))

gnome-flashback-compiz.session: Makefile.am gnome-flashback.session.in
	$(AM_V_GEN) sed \
		-e 's,[@]NAME[@],Compiz,g' \
		-e 's,[@]REQUIRED_COMPONENTS[@],$(compiz),g' \
		< $(srcdir)/gnome-flashback.session.in > $@

gnome-flashback-metacity.session: Makefile.am gnome-flashback.session.in
	$(AM_V_GEN) sed \
		-e 's,[@]NAME[@],Metacity,g' \
		-e 's,[@]REQUIRED_COMPONENTS[@],$(metacity),g' \
		< $(srcdir)/gnome-flashback.session.in > $@

sessions = \
	gnome-flashback-compiz.session \
	gnome-flashback-metacity.session \
	$(NULL)

sessiondir = $(datadir)/gnome-session/sessions
session_DATA = $(sessions)

CLEANFILES = \
	$(sessions) \
	$(NULL)

EXTRA_DIST = \
	gnome-flashback.session.in \
	$(NULL)

-include $(top_srcdir)/git.mk
