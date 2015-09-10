NULL =

noinst_LTLIBRARIES = \
	libscreenshot.la \
	$(NULL)

libscreenshot_la_CFLAGS = \
	$(SCREENSHOT_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libscreenshot \
	$(NULL)

libscreenshot_la_SOURCES = \
	gf-flashspot.c \
	gf-flashspot.h \
	gf-screenshot.c \
	gf-screenshot.h \
	gf-select-area.c \
	gf-select-area.h \
	$(BUILT_SOURCES) \
	$(NULL)

libscreenshot_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreenshot_la_LIBADD = \
	$(SCREENSHOT_LIBS) \
	$(NULL)

gf-dbus-screenshot.h:
gf-dbus-screenshot.c: org.gnome.Shell.Screenshot.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Shell. \
		--c-namespace Gf \
		--generate-c-code gf-dbus-screenshot \
		--annotate "org.gnome.Shell.Screenshot" \
		           "org.gtk.GDBus.C.Name" DBusScreenshot \
		$(srcdir)/org.gnome.Shell.Screenshot.xml

BUILT_SOURCES = \
	gf-dbus-screenshot.c \
	gf-dbus-screenshot.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Shell.Screenshot.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
