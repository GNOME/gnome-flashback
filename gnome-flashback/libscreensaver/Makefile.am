NULL =

noinst_LTLIBRARIES = \
	libscreensaver.la \
	$(NULL)

libscreensaver_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"screensaver\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-I$(top_builddir)/gnome-flashback/libscreensaver \
	-I$(top_srcdir) \
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
	gf-grab.c \
	gf-grab.h \
	gf-info-bar.c \
	gf-info-bar.h \
	gf-listener.c \
	gf-listener.h \
	gf-panel.c \
	gf-panel.h \
	gf-prefs.c \
	gf-prefs.h \
	gf-screensaver.c \
	gf-screensaver.h \
	gf-screensaver-utils.c \
	gf-screensaver-utils.h \
	gf-user-image.c \
	gf-user-image.h \
	gf-watcher.c \
	gf-watcher.h \
	$(BUILT_SOURCES) \
	$(NULL)

libscreensaver_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreensaver_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(SCREENSAVER_LIBS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gf-auth.h \
	$(NULL)

gf-screensaver-enum-types.c: gf-screensaver-enum-types.c.in gf-screensaver-enum-types.h $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-screensaver-enum-types.c.in $(ENUM_TYPES) > \
		gf-screensaver-enum-types.c.tmp && mv gf-screensaver-enum-types.c.tmp gf-screensaver-enum-types.c

gf-screensaver-enum-types.h: gf-screensaver-enum-types.h.in $(ENUM_TYPES)
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
