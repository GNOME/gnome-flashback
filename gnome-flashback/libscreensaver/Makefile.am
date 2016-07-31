NULL =

noinst_LTLIBRARIES = \
	libscreensaver.la \
	$(NULL)

libscreensaver_la_CPPFLAGS = \
	-I$(top_builddir)/gnome-flashback/libscreensaver \
	$(NULL)

libscreensaver_la_CFLAGS = \
	$(SCREENSAVER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libscreensaver_la_SOURCES = \
	gf-screensaver.c \
	gf-screensaver.h \
	$(BUILT_SOURCES) \
	$(NULL)

libscreensaver_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreensaver_la_LIBADD = \
	$(SCREENSAVER_LIBS) \
	$(NULL)

gf-screensaver-gen.h:
gf-screensaver-gen.c: org.gnome.ScreenSaver.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix org.gnome.ScreenSaver. \
		--generate-c-code gf-screensaver-gen \
		--c-namespace Gf \
		--annotate "org.gnome.ScreenSaver" \
		           "org.gtk.GDBus.C.Name" ScreensaverGen \
		$(srcdir)/org.gnome.ScreenSaver.xml

BUILT_SOURCES = \
	gf-screensaver-gen.c \
	gf-screensaver-gen.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.ScreenSaver.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
