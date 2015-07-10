NULL =

noinst_LTLIBRARIES = \
	libscreencast.la \
	$(NULL)

libscreencast_la_CFLAGS = \
	$(SCREENCAST_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libscreencast \
	$(NULL)

libscreencast_la_SOURCES = \
	flashback-dbus-screencast.c \
	flashback-dbus-screencast.h \
	flashback-screencast.c \
	flashback-screencast.h \
	$(NULL)

libscreencast_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreencast_la_LIBADD = \
	$(SCREENCAST_LIBS) \
	$(NULL)

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
	flashback-dbus-screencast.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Shell.Screencast.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
