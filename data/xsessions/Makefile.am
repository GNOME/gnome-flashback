NULL =

xsession_in_in_files = \
	gnome-flashback-metacity.desktop.in.in \
	$(NULL)

if WITH_COMPIZ_SESSION
xsession_in_in_files += \
	gnome-flashback-compiz.desktop.in.in \
	$(NULL)
endif

xsessiondir = $(datadir)/xsessions
xsession_in_files = $(xsession_in_in_files:.desktop.in.in=.desktop.in)
xsession_DATA = $(xsession_in_files:.desktop.in=.desktop)

%.desktop.in: %.desktop.in.in
	$(AM_V_GEN) sed -e 's,[@]libexecdir[@],$(libexecdir),g' < $< > $@

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t \
		-d $(top_srcdir)/po && mv $@-t $@

EXTRA_DIST = \
	gnome-flashback-compiz.desktop.in.in \
	gnome-flashback-metacity.desktop.in.in \
	$(NULL)

CLEANFILES = \
	$(xsession_in_files) \
	$(xsession_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
