NULL =

service_in_files = gnome-flashback.service.in
service_files = $(service_in_files:.service.in=.service)

systemduserdir = $(libdir)/systemd/user
systemduser_DATA = \
	gnome-flashback.target \
	gnome-session-x11@gnome-flashback-compiz.target \
	gnome-session-x11@gnome-flashback-metacity.target \
	$(service_files) \
	$(NULL)

%.service: %.service.in
	$(AM_V_GEN) sed -e 's,[@]bindir[@],$(bindir),g' < $< > $@

EXTRA_DIST = \
	$(service_in_files) \
	$(systemduser_DATA) \
	$(NULL)

CLEANFILES = \
	$(service_files) \
	$(xsession_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
