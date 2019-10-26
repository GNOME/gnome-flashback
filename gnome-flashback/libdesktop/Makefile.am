NULL =

noinst_LTLIBRARIES = \
	libdesktop.la \
	$(NULL)

libdesktop_la_CPPFLAGS = \
	-I$(top_builddir)/gnome-flashback/libdesktop \
	$(NULL)

libdesktop_la_CFLAGS = \
	$(DESKTOP_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libdesktop_la_SOURCES = \
	gf-desktop-window.c \
	gf-desktop-window.h \
	gf-desktop.c \
	gf-desktop.h \
	$(NULL)

libdesktop_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdesktop_la_LIBADD = \
	$(DESKTOP_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
