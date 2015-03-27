noinst_LTLIBRARIES = \
	libscreencast.la

AM_CPPFLAGS = \
	$(SCREENCAST_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libscreencast

libscreencast_la_SOURCES = \
	flashback-dbus-screencast.c \
	flashback-dbus-screencast.h \
	flashback-screencast.c \
	flashback-screencast.h

libscreencast_la_LIBADD = \
	$(SCREENCAST_LIBS)

flashback-dbus-screencast.h:
flashback-dbus-screencast.c: org.gnome.Shell.Screencast.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Shell. \
		--c-namespace Flashback \
		--generate-c-code flashback-dbus-screencast \
		--annotate "org.gnome.Shell.Screencast" \
		           "org.gtk.GDBus.C.Name" DBusScreencast \
		$(srcdir)/org.gnome.Shell.Screencast.xml

BUILT_SOURCES = \
	flashback-dbus-screencast.c \
	flashback-dbus-screencast.h

EXTRA_DIST = \
	org.gnome.Shell.Screencast.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
