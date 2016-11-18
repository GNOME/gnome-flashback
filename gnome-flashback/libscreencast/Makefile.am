NULL =

noinst_LTLIBRARIES = \
	libscreencast.la \
	$(NULL)

libscreencast_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"screencast\" \
	-DG_LOG_USE_STRUCTURED=1 \
	$(NULL)

libscreencast_la_CFLAGS = \
	$(SCREENCAST_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libscreencast \
	$(NULL)

libscreencast_la_SOURCES = \
	gf-screencast.c \
	gf-screencast.h \
	$(BUILT_SOURCES)
	$(NULL)

libscreencast_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreencast_la_LIBADD = \
	$(SCREENCAST_LIBS) \
	$(NULL)

gf-dbus-screencast.h:
gf-dbus-screencast.c: org.gnome.Shell.Screencast.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Shell. \
		--c-namespace Gf \
		--generate-c-code gf-dbus-screencast \
		--annotate "org.gnome.Shell.Screencast" \
		           "org.gtk.GDBus.C.Name" DBusScreencast \
		$(srcdir)/org.gnome.Shell.Screencast.xml

BUILT_SOURCES = \
	gf-dbus-screencast.c \
	gf-dbus-screencast.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Shell.Screencast.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
