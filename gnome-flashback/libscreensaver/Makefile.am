NULL =

noinst_LTLIBRARIES = \
	libscreensaver.la \
	$(NULL)

libscreensaver_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"screensaver\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-I$(top_builddir)/backends \
	-I$(top_builddir)/gnome-flashback/libcommon \
	-I$(top_builddir)/gnome-flashback/libinput-sources \
	-I$(top_builddir)/gnome-flashback/libscreensaver \
	-I$(top_srcdir) \
	-I$(top_srcdir)/gnome-flashback \
	$(AM_CPPFLAGS) \
	$(NULL)

libscreensaver_la_CFLAGS = \
	$(SCREENSAVER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libscreensaver_la_SOURCES = \
	gf-auth.c \
	gf-auth.h \
	gf-fade.c \
	gf-fade.h \
	gf-grab.c \
	gf-grab.h \
	gf-info-bar.c \
	gf-info-bar.h \
	gf-listener.c \
	gf-listener.h \
	gf-manager.c \
	gf-manager.h \
	gf-panel.c \
	gf-panel.h \
	gf-prefs.c \
	gf-prefs.h \
	gf-screensaver.c \
	gf-screensaver.h \
	gf-screensaver-utils.c \
	gf-screensaver-utils.h \
	gf-unlock-dialog.c \
	gf-unlock-dialog.h \
	gf-user-image.c \
	gf-user-image.h \
	gf-watcher.c \
	gf-watcher.h \
	gf-window.c \
	gf-window.h \
	$(BUILT_SOURCES) \
	$(NULL)

libscreensaver_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreensaver_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(top_builddir)/backends/libbackends.la \
	$(top_builddir)/gnome-flashback/libcommon/libcommon.la \
	$(top_builddir)/gnome-flashback/libinput-sources/libinput-sources.la \
	$(SCREENSAVER_LIBS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gf-auth.h \
	$(srcdir)/gf-unlock-dialog.h \
	$(NULL)

gf-screensaver-enum-types.c: gf-screensaver-enum-types.c.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-screensaver-enum-types.c.in $(ENUM_TYPES) > \
		gf-screensaver-enum-types.c.tmp && mv gf-screensaver-enum-types.c.tmp gf-screensaver-enum-types.c

gf-screensaver-enum-types.h: gf-screensaver-enum-types.h.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-screensaver-enum-types.h.in $(ENUM_TYPES) > \
		gf-screensaver-enum-types.h.tmp && mv gf-screensaver-enum-types.h.tmp gf-screensaver-enum-types.h

BUILT_SOURCES = \
	gf-screensaver-enum-types.c \
	gf-screensaver-enum-types.h \
	$(NULL)

EXTRA_DIST = \
	gf-screensaver-enum-types.c.in \
	gf-screensaver-enum-types.h.in \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
