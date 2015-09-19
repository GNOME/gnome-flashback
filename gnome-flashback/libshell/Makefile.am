NULL =

noinst_LTLIBRARIES = \
	libshell.la \
	$(NULL)

libshell_la_CFLAGS = \
	$(SHELL_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(srcdir)/../../gnome-flashback \
	-I$(top_builddir)/gnome-flashback \
	-I$(top_builddir)/gnome-flashback/libcommon \
	-I$(top_builddir)/gnome-flashback/libshell \
	$(NULL)

libshell_la_SOURCES = \
	flashback-dbus-shell.c \
	flashback-dbus-shell.h \
	flashback-label-window.c \
	flashback-label-window.h \
	flashback-monitor-labeler.c \
	flashback-monitor-labeler.h \
	flashback-osd.c \
	flashback-osd.h \
	flashback-osd-window.c \
	flashback-osd-window.h \
	flashback-shell.c \
	flashback-shell.h \
	$(NULL)

libshell_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libshell_la_LIBADD = \
	$(SHELL_LIBS) \
	$(top_builddir)/gnome-flashback/libcommon/libcommon.la \
	$(NULL)

flashback-dbus-shell.h:
flashback-dbus-shell.c: org.gnome.Shell.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Shell. \
		--c-namespace Flashback \
		--generate-c-code flashback-dbus-shell \
		$(srcdir)/org.gnome.Shell.xml

BUILT_SOURCES = \
	flashback-dbus-shell.c \
	flashback-dbus-shell.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Shell.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
