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
	flashback-dbus-screenshot.c \
	flashback-dbus-screenshot.h \
	flashback-screenshot.c \
	flashback-screenshot.h \
	flashback-select-area.c \
	flashback-select-area.h \
	$(NULL)

libscreenshot_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreenshot_la_LIBADD = \
	$(SCREENSHOT_LIBS) \
	$(NULL)

flashback-dbus-screenshot.h:
flashback-dbus-screenshot.c: org.gnome.Shell.Screenshot.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Shell. \
		--c-namespace Flashback \
		--generate-c-code flashback-dbus-screenshot \
		--annotate "org.gnome.Shell.Screenshot" \
		           "org.gtk.GDBus.C.Name" DBusScreenshot \
		$(srcdir)/org.gnome.Shell.Screenshot.xml

BUILT_SOURCES = \
	flashback-dbus-screenshot.c \
	flashback-dbus-screenshot.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Shell.Screenshot.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
