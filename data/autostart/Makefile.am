NULL =

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = \
	gnome-flashback-clipboard.desktop \
	gnome-flashback-geoclue-demo-agent.desktop \
	gnome-flashback-idle-monitor.desktop \
	gnome-flashback-media-keys.desktop \
	gnome-flashback-nm-applet.desktop \
	gnome-flashback-polkit.desktop \
	$(NULL)

%.desktop.in: %.desktop.in.in
	$(AM_V_GEN) sed -e 's,[@]libexecdir[@],$(libexecdir),g' < $< > $@

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t \
		-d $(top_srcdir)/po && mv $@-t $@

EXTRA_DIST = \
	gnome-flashback-clipboard.desktop.in.in \
	gnome-flashback-geoclue-demo-agent.desktop.in.in \
	gnome-flashback-idle-monitor.desktop.in.in \
	gnome-flashback-media-keys.desktop.in.in \
	gnome-flashback-nm-applet.desktop.in \
	gnome-flashback-polkit.desktop.in.in \
	$(NULL)

CLEANFILES = \
	gnome-flashback-clipboard.desktop.in \
	gnome-flashback-clipboard.desktop \
	gnome-flashback-geoclue-demo-agent.desktop.in \
	gnome-flashback-geoclue-demo-agent.desktop \
	gnome-flashback-idle-monitor.desktop.in \
	gnome-flashback-idle-monitor.desktop \
	gnome-flashback-media-keys.desktop.in \
	gnome-flashback-media-keys.desktop \
	gnome-flashback-nm-applet.desktop \
	gnome-flashback-polkit.desktop.in \
	gnome-flashback-polkit.desktop \
	$(NULL)

-include $(top_srcdir)/git.mk
