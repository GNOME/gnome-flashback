noinst_LTLIBRARIES = \
	libshell.la

AM_CPPFLAGS = \
	$(SHELL_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libshell

libshell_la_SOURCES = \
	flashback-dbus-shell.c \
	flashback-dbus-shell.h \
	flashback-key-bindings.c \
	flashback-key-bindings.h \
	flashback-monitor-labeler.c \
	flashback-monitor-labeler.h \
	flashback-osd.c \
	flashback-osd.h \
	flashback-shell.c \
	flashback-shell.h

libshell_la_LIBADD = \
	$(SHELL_LIBS)

flashback-dbus-shell.h:
flashback-dbus-shell.c: org.gnome.Shell.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Shell. \
		--c-namespace Flashback \
		--generate-c-code flashback-dbus-shell \
		$(srcdir)/org.gnome.Shell.xml

BUILT_SOURCES = \
	flashback-dbus-shell.c \
	flashback-dbus-shell.h

EXTRA_DIST = \
	org.gnome.Shell.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
