NULL =

if ENABLE_SYSTEMD_SESSION

service_in_files = gnome-flashback.service.in
service_files = $(service_in_files:.service.in=.service)

target_files = \
	gnome-flashback.target \
	gnome-session-x11@gnome-flashback-compiz.target \
	gnome-session-x11@gnome-flashback-metacity.target \
	$(NULL)

systemduserdir = $(libdir)/systemd/user
systemduser_DATA = \
	$(service_files) \
	$(target_files) \
	$(NULL)

%.service: %.service.in
	$(AM_V_GEN) sed -e 's,[@]bindir[@],$(bindir),g' < $< > $@

CLEANFILES = \
	$(service_files) \
	$(NULL)

endif

EXTRA_DIST = \
	$(service_in_files) \
	$(target_files) \
	$(NULL)

-include $(top_srcdir)/git.mk
