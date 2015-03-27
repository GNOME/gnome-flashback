noinst_LTLIBRARIES = \
	libscreenshot.la

AM_CPPFLAGS = \
	$(SCREENSHOT_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libscreenshot

libscreenshot_la_SOURCES = \
	flashback-dbus-screenshot.c \
	flashback-dbus-screenshot.h \
	flashback-screenshot.c \
	flashback-screenshot.h

libscreenshot_la_LIBADD = \
	$(SCREENSHOT_LIBS)

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
	flashback-dbus-screenshot.h

EXTRA_DIST = \
	org.gnome.Shell.Screenshot.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
