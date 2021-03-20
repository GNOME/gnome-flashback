NULL =

noinst_LTLIBRARIES = \
	libdesktop.la \
	$(NULL)

libdesktop_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"desktop\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-I$(top_builddir)/backends \
	-I$(top_builddir)/gnome-flashback/libcommon \
	-I$(top_builddir)/gnome-flashback/libdesktop \
	-I$(top_srcdir)/gnome-flashback \
	-I$(top_srcdir) \
	$(NULL)

libdesktop_la_CFLAGS = \
	$(DESKTOP_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libdesktop_la_SOURCES = \
	gf-background.c \
	gf-background.h \
	gf-create-folder-dialog.c \
	gf-create-folder-dialog.h \
	gf-desktop-enums.h \
	gf-desktop-window.c \
	gf-desktop-window.h \
	gf-desktop.c \
	gf-desktop.h \
	gf-dummy-icon.c \
	gf-dummy-icon.h \
	gf-home-icon.c \
	gf-home-icon.h \
	gf-trash-icon.c \
	gf-trash-icon.h \
	gf-icon-view.c \
	gf-icon-view.h \
	gf-icon.c \
	gf-icon.h \
	gf-monitor-view.c \
	gf-monitor-view.h \
	gf-rename-popover.c \
	gf-rename-popover.h \
	gf-thumbnail-factory.c \
	gf-thumbnail-factory.h \
	gf-utils.c \
	gf-utils.h \
	gf-workarea-watcher.c \
	gf-workarea-watcher.h \
	$(BUILT_SOURCES) \
	$(NULL)

libdesktop_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdesktop_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(top_builddir)/backends/libbackends.la \
	$(top_builddir)/gnome-flashback/libcommon/libcommon.la \
	$(DESKTOP_LIBS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gf-desktop-enums.h \
	$(NULL)

gf-desktop-enum-types.c: gf-desktop-enum-types.c.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-desktop-enum-types.c.in $(ENUM_TYPES) > \
		gf-desktop-enum-types.c.tmp && mv gf-desktop-enum-types.c.tmp gf-desktop-enum-types.c

gf-desktop-enum-types.h: gf-desktop-enum-types.h.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-desktop-enum-types.h.in $(ENUM_TYPES) > \
		gf-desktop-enum-types.h.tmp && mv gf-desktop-enum-types.h.tmp gf-desktop-enum-types.h

BUILT_SOURCES = \
	gf-desktop-enum-types.c \
	gf-desktop-enum-types.h \
	$(NULL)

EXTRA_DIST = \
	gf-desktop-enum-types.c.in \
	gf-desktop-enum-types.h.in \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
