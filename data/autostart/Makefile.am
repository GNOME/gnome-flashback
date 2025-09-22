NULL =

autostartdir = $(sysconfdir)/xdg/autostart
autostart_DATA = \
	gnome-flashback-geoclue-demo-agent.desktop \
	gnome-flashback-nm-applet.desktop \
	$(NULL)

%.desktop.in: %.desktop.in.in
	$(AM_V_GEN) sed -e 's,[@]libexecdir[@],$(libexecdir),g' < $< > $@

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t \
		-d $(top_srcdir)/po && mv $@-t $@

EXTRA_DIST = \
	gnome-flashback-geoclue-demo-agent.desktop.in.in \
	gnome-flashback-nm-applet.desktop.in \
	$(NULL)

CLEANFILES = \
	gnome-flashback-geoclue-demo-agent.desktop.in \
	gnome-flashback-geoclue-demo-agent.desktop \
	gnome-flashback-nm-applet.desktop \
	$(NULL)

-include $(top_srcdir)/git.mk
