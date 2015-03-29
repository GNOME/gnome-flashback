SUBDIRS = \
	libautomount-manager \
	libdesktop-background \
	libdisplay-config \
	libend-session-dialog \
	libscreencast \
	libscreenshot \
	libshell \
	libsound-applet

bin_PROGRAMS = \
	gnome-flashback

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	-I$(top_builddir)/gnome-flashback \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

gnome_flashback_SOURCES = \
	flashback-application.c \
	flashback-application.h \
	flashback-main.c \
	flashback-session.c \
	flashback-session.h \
	$(BUILT_SOURCES)

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS) \
	$(top_builddir)/gnome-flashback/libautomount-manager/libautomount-manager.la \
	$(top_builddir)/gnome-flashback/libdesktop-background/libdesktop-background.la \
	$(top_builddir)/gnome-flashback/libdisplay-config/libdisplay-config.la \
	$(top_builddir)/gnome-flashback/libend-session-dialog/libend-session-dialog.la \
	$(top_builddir)/gnome-flashback/libscreencast/libscreencast.la \
	$(top_builddir)/gnome-flashback/libscreenshot/libscreenshot.la \
	$(top_builddir)/gnome-flashback/libshell/libshell.la \
	$(top_builddir)/gnome-flashback/libsound-applet/libsound-applet.la

flashback-resources.c: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<

flashback-resources.h: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<

BUILT_SOURCES = \
	flashback-resources.c \
	flashback-resources.h

EXTRA_DIST = \
	Adwaita.css \
	flashback.gresource.xml \
	HighContrast.css

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
