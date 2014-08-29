noinst_LTLIBRARIES = \
	libdesktop-background.la

AM_CPPFLAGS = \
	$(DESKTOP_BACKGROUND_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libdesktop-background \
	-DGNOME_DESKTOP_USE_UNSTABLE_API

libdesktop_background_la_SOURCES = \
	flashback-desktop-background.c \
	flashback-desktop-background.h

libdesktop_background_la_LIBADD = \
	$(DESKTOP_BACKGROUND_LIBS)

-include $(top_srcdir)/git.mk
