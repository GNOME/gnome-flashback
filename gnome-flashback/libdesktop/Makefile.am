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
	gf-desktop-window.c \
	gf-desktop-window.h \
	gf-desktop.c \
	gf-desktop.h \
	gf-icon-view.c \
	gf-icon-view.h \
	$(NULL)

libdesktop_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdesktop_la_LIBADD = \
	$(DESKTOP_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk