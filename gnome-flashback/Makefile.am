NULL =

SUBDIRS = \
	libautomount-manager \
	libdesktop-background \
	libdisplay-config \
	libend-session-dialog \
	libidle-monitor \
	libpolkit \
	libscreencast \
	libscreenshot \
	libshell \
	libsound-applet \
	libworkarounds \
	$(NULL)

bin_PROGRAMS = \
	gnome-flashback \
	$(NULL)

gnome_flashback_CFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(NULL)

gnome_flashback_SOURCES = \
	flashback-application.c \
	flashback-application.h \
	flashback-main.c \
	gf-session.c \
	gf-session.h \
	$(BUILT_SOURCES) \
	$(NULL)

gnome_flashback_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS) \
	$(top_builddir)/gnome-flashback/libautomount-manager/libautomount-manager.la \
	$(top_builddir)/gnome-flashback/libdesktop-background/libdesktop-background.la \
	$(top_builddir)/gnome-flashback/libdisplay-config/libdisplay-config.la \
	$(top_builddir)/gnome-flashback/libend-session-dialog/libend-session-dialog.la \
	$(top_builddir)/gnome-flashback/libidle-monitor/libidle-monitor.la \
	$(top_builddir)/gnome-flashback/libpolkit/libpolkit.la \
	$(top_builddir)/gnome-flashback/libscreencast/libscreencast.la \
	$(top_builddir)/gnome-flashback/libscreenshot/libscreenshot.la \
	$(top_builddir)/gnome-flashback/libshell/libshell.la \
	$(top_builddir)/gnome-flashback/libsound-applet/libsound-applet.la \
	$(top_builddir)/gnome-flashback/libworkarounds/libworkarounds.la \
	$(NULL)

flashback-resources.c: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<

flashback-resources.h: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<

BUILT_SOURCES = \
	flashback-resources.c \
	flashback-resources.h \
	$(NULL)

EXTRA_DIST = \
	Adwaita.css \
	flashback.gresource.xml \
	HighContrast.css \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
