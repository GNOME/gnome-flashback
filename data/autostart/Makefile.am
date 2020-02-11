NULL =

autostart_in_files = \
	gnome-flashback-nm-applet.desktop.in \
	$(NULL)

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t \
		-d $(top_srcdir)/po && mv $@-t $@

EXTRA_DIST = \
	$(autostart_in_files) \
	$(NULL)

CLEANFILES = \
	$(autostart_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
