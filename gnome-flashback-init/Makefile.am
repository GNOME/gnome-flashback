bin_PROGRAMS = \
	gnome-flashback-init

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_INIT_CFLAGS) \
	-I$(top_builddir)/gnome-flashback-init

gnome_flashback_init_SOURCES = \
	main.c

gnome_flashback_init_LDADD = \
	$(GNOME_FLASHBACK_INIT_LIBS)

desktopdir       = $(datadir)/applications
desktop_in_files = gnome-flashback-init.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

@INTLTOOL_XML_NOMERGE_RULE@

EXTRA_DIST = \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA)

-include $(top_srcdir)/git.mk
