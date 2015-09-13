NULL =

noinst_LTLIBRARIES = \
	libdesktop-background.la \
	$(NULL)

libdesktop_background_la_CFLAGS = \
	$(DESKTOP_BACKGROUND_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libdesktop-background \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	$(NULL)

libdesktop_background_la_SOURCES = \
	gf-desktop-background.c \
	gf-desktop-background.h \
	gf-desktop-window.c \
	gf-desktop-window.h \
	$(NULL)

libdesktop_background_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdesktop_background_la_LIBADD = \
	$(DESKTOP_BACKGROUND_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
