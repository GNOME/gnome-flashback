NULL =

desktop_in_in_files = \
	gnome-flashback.desktop.in.in \
	$(NULL)

desktopdir = $(datadir)/applications
desktop_in_files = $(desktop_in_in_files:.desktop.in.in=.desktop.in)
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

%.desktop.in: %.desktop.in.in
	$(AM_V_GEN) sed -e 's,[@]hidden_under_systemd[@],$(HIDDEN_UNDER_SYSTEMD),g' < $< > $@

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t \
		-d $(top_srcdir)/po && mv $@-t $@

EXTRA_DIST = \
	$(desktop_in_in_files) \
	$(NULL)

CLEANFILES = \
	$(desktop_in_files) \
	$(desktop_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
