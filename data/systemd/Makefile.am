NULL =

required_components_gsd = \
	org.gnome.SettingsDaemon.A11ySettings \
	org.gnome.SettingsDaemon.Color \
	org.gnome.SettingsDaemon.Datetime \
	org.gnome.SettingsDaemon.Housekeeping \
	org.gnome.SettingsDaemon.Keyboard \
	org.gnome.SettingsDaemon.MediaKeys \
	org.gnome.SettingsDaemon.Power \
	org.gnome.SettingsDaemon.PrintNotifications \
	org.gnome.SettingsDaemon.Rfkill \
	org.gnome.SettingsDaemon.ScreensaverProxy \
	org.gnome.SettingsDaemon.Sharing \
	org.gnome.SettingsDaemon.Smartcard \
	org.gnome.SettingsDaemon.Sound \
	org.gnome.SettingsDaemon.UsbProtection \
	org.gnome.SettingsDaemon.Wwan \
	org.gnome.SettingsDaemon.XSettings \
	$(NULL)

service_in_files = gnome-flashback.service.in
service_files = $(service_in_files:.service.in=.service)

target_files = \
	gnome-flashback.target \
	gnome-session-x11@gnome-flashback-metacity.target \
	$(NULL)

systemduserunitdir = $(prefix)/lib/systemd/user
systemduserunit_DATA = \
	$(service_files) \
	$(target_files) \
	$(NULL)

%.service: %.service.in
	$(AM_V_GEN) sed -e 's,[@]bindir[@],$(bindir),g' < $< > $@

space = $(NULL) $(NULL)
wants_gsd = Wants=$(subst $(space),.target\nWants=,$(strip $(required_components_gsd))).target

metacity/session.conf: Makefile.am gnome-flashback.session.conf.in
	$(AM_V_GEN) mkdir -p `dirname $@` && sed \
		-e 's,[@]WANTS_GSD[@],$(wants_gsd),g' \
		< $(srcdir)/gnome-flashback.session.conf.in > $@

systemd_metacity_sessiondir = $(prefix)/lib/systemd/user/gnome-session@gnome-flashback-metacity.target.d
systemd_metacity_session_DATA = \
	metacity/session.conf \
	$(NULL)

CLEANFILES = \
	$(service_files) \
	$(systemd_metacity_session_DATA) \
	$(NULL)

EXTRA_DIST = \
	gnome-flashback.session.conf.in \
	$(service_in_files) \
	$(target_files) \
	$(NULL)

if WITH_COMPIZ_SESSION
compiz_target_files = \
	gnome-session-x11@gnome-flashback-compiz.target \
	$(NULL)

systemduserunit_DATA += \
	$(compiz_target_files) \
	$(NULL)

compiz/session.conf: Makefile.am gnome-flashback.session.conf.in
	$(AM_V_GEN) mkdir -p `dirname $@` && sed \
		-e 's,[@]WANTS_GSD[@],$(wants_gsd),g' \
		< $(srcdir)/gnome-flashback.session.conf.in > $@

systemd_compiz_sessiondir = $(prefix)/lib/systemd/user/gnome-session@gnome-flashback-compiz.target.d
systemd_compiz_session_DATA = \
	compiz/session.conf \
	$(NULL)

CLEANFILES += \
	$(systemd_compiz_session_DATA) \
	$(NULL)

EXTRA_DIST += \
	$(compiz_target_files) \
	$(NULL)
endif

-include $(top_srcdir)/git.mk
