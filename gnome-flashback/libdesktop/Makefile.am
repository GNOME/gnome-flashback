NULL =

noinst_LTLIBRARIES = \
	libdesktop.la \
	$(NULL)

libdesktop_la_CPPFLAGS = \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-I$(top_builddir)/gnome-flashback/libdesktop \
	$(NULL)

libdesktop_la_CFLAGS = \
	$(DESKTOP_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libdesktop_la_SOURCES = \
	gf-background.c \
	gf-background.h \
	gf-desktop-enums.h \
	gf-desktop-window.c \
	gf-desktop-window.h \
	gf-desktop.c \
	gf-desktop.h \
	gf-icon-view.c \
	gf-icon-view.h \
	gf-icon.c \
	gf-icon.h \
	gf-monitor-view.c \
	gf-monitor-view.h \
	gf-utils.c \
	gf-utils.h \
	$(BUILT_SOURCES) \
	$(NULL)

libdesktop_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdesktop_la_LIBADD = \
	$(DESKTOP_LIBS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gf-desktop-enums.h \
	$(srcdir)/gf-icon.h \
	$(NULL)

gf-desktop-enum-types.c: gf-desktop-enum-types.c.in gf-desktop-enum-types.h $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-desktop-enum-types.c.in $(ENUM_TYPES) > \
		gf-desktop-enum-types.c.tmp && mv gf-desktop-enum-types.c.tmp gf-desktop-enum-types.c

gf-desktop-enum-types.h: gf-desktop-enum-types.h.in $(ENUM_TYPES)
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
