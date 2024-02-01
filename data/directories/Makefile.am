NULL =

directory_in_files = \
	X-GNOME-Flashback-Science.directory.desktop.in \
	X-GNOME-Flashback-Settings.directory.desktop.in \
	X-GNOME-Flashback-Settings-System.directory.desktop.in \
	$(NULL)

directorydir = $(datadir)/desktop-directories
directory_DATA = $(directory_in_files:.directory.desktop.in=.directory)

%.directory: %.directory.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t \
		-d $(top_srcdir)/po && mv $@-t $@

EXTRA_DIST = \
	$(directory_in_files) \
	$(NULL)

CLEANFILES = \
	$(directory_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
