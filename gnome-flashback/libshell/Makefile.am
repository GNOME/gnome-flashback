NULL =

noinst_LTLIBRARIES = \
	libshell.la \
	$(NULL)

libshell_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"shell\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_builddir)/gnome-flashback/libcommon \
	-I$(top_builddir)/gnome-flashback/libshell \
	-I$(top_srcdir)/gnome-flashback \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libshell_la_CFLAGS = \
	$(SHELL_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libshell_la_SOURCES = \
	flashback-monitor-labeler.c \
	flashback-monitor-labeler.h \
	flashback-osd.c \
	flashback-osd.h \
	flashback-shell.c \
	flashback-shell.h \
	gf-label-window.c \
	gf-label-window.h \
	gf-osd-window.c \
	gf-osd-window.h \
	gf-shell-introspect.c \
	gf-shell-introspect.h \
	$(NULL)

libshell_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libshell_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(top_builddir)/gnome-flashback/libcommon/libcommon.la \
	$(SHELL_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
