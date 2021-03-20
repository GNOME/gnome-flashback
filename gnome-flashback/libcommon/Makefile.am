NULL =

noinst_LTLIBRARIES = \
	libcommon.la \
	$(NULL)

libcommon_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"common\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-I$(top_builddir)/gnome-flashback/libcommon \
	$(NULL)

libcommon_la_CFLAGS = \
	$(COMMON_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libcommon_la_SOURCES = \
	gf-bg.c \
	gf-bg.h \
	gf-keybindings.c \
	gf-keybindings.h \
	gf-popup-window.c \
	gf-popup-window.h \
	$(BUILT_SOURCES) \
	$(NULL)

libcommon_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libcommon_la_LIBADD = \
	$(COMMON_LIBS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gf-keybindings.h \
	$(NULL)

gf-common-enum-types.c: gf-common-enum-types.c.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-common-enum-types.c.in $(ENUM_TYPES) > \
		gf-common-enum-types.c.tmp && mv gf-common-enum-types.c.tmp gf-common-enum-types.c

gf-common-enum-types.h: gf-common-enum-types.h.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-common-enum-types.h.in $(ENUM_TYPES) > \
		gf-common-enum-types.h.tmp && mv gf-common-enum-types.h.tmp gf-common-enum-types.h

BUILT_SOURCES = \
	gf-common-enum-types.c \
	gf-common-enum-types.h \
	$(NULL)

EXTRA_DIST = \
	gf-common-enum-types.c.in \
	gf-common-enum-types.h.in \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
