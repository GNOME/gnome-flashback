NULL =

SUBDIRS = \
	applications \
	autostart \
	sessions \
	xsessions \
	$(NULL)

libexec_SCRIPTS = \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	$(NULL)

gnome-flashback-compiz: Makefile
	$(AM_V_GEN) (echo '#! /bin/sh' && echo 'exec gnome-session --session=gnome-flashback-compiz "$$@"') > $@
	$(AM_V_at) chmod a+x $@

gnome-flashback-metacity: Makefile
	$(AM_V_GEN) (echo '#! /bin/sh' && echo 'exec gnome-session --session=gnome-flashback-metacity --disable-acceleration-check "$$@"') > $@
	$(AM_V_at) chmod a+x $@

menudir   = $(sysconfdir)/xdg/menus
menu_DATA = gnome-flashback-applications.menu

directorydir = $(datadir)/desktop-directories

directory_in_files =					\
	X-GNOME-Flashback-Settings.directory.desktop.in		\
	X-GNOME-Flashback-Settings-System.directory.desktop.in

directory_DATA = $(directory_in_files:.directory.desktop.in=.directory)

%.directory: %.directory.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -o $@-t -d $(top_srcdir)/po && mv $@-t $@

gsettings_SCHEMAS = org.gnome.gnome-flashback.gschema.xml
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(menu_DATA) \
	$(gsettings_SCHEMAS) \
	$(directory_in_files) \
	$(NULL)

CLEANFILES = \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	*.gschema.valid \
	$(directory_DATA) \
	$(NULL)

-include $(top_srcdir)/git.mk
