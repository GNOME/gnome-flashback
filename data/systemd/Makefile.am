NULL =

if ENABLE_SYSTEMD_SESSION

service_in_files = gnome-flashback.service.in
service_files = $(service_in_files:.service.in=.service)

target_files = \
	gnome-flashback.target \
	$(NULL)

systemduserunitdir = $(prefix)/lib/systemd/user
systemduserunit_DATA = \
	$(service_files) \
	$(target_files) \
	$(NULL)

%.service: %.service.in
	$(AM_V_GEN) sed -e 's,[@]bindir[@],$(bindir),g' < $< > $@

space = $(NULL) $(NULL)
wants_gsd = Wants=$(subst $(space),.target\nWants=,$(strip $(REQUIRED_COMPNENTS_GSD))).target

metacity/session.conf: ../../configure.ac Makefile.am gnome-flashback.session.conf.in
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

if WITH_COMPIZ_SESSION
compiz/session.conf: ../../configure.ac Makefile.am gnome-flashback.session.conf.in
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
endif

endif

EXTRA_DIST = \
	gnome-flashback.session.conf.in \
	$(service_in_files) \
	$(target_files) \
	$(NULL)

-include $(top_srcdir)/git.mk
