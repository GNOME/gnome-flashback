bin_PROGRAMS = \
	gnome-flashback

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	-I$(top_builddir)/gnome-flashback \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

gnome_flashback_SOURCES = \
	flashback-application.c \
	flashback-application.h \
	flashback-main.c

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS)

desktopdir       = $(datadir)/applications
desktop_in_files = gnome-flashback.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

EXTRA_DIST = \
	$(desktop_in_files)

CLEANFILES = \
	$(desktop_DATA)

-include $(top_srcdir)/git.mk
