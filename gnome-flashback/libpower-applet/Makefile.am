NULL =

noinst_LTLIBRARIES = \
	libpower-applet.la \
	$(NULL)

libpower_applet_la_CFLAGS = \
	$(POWER_APPLET_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libpower-applet \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(NULL)

libpower_applet_la_SOURCES = \
  gf-power-applet.c \
	gf-power-applet.h \
	$(NULL)

libpower_applet_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libpower_applet_la_LIBADD = \
	$(POWER_APPLET_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
