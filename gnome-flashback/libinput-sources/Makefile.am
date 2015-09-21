NULL =

noinst_LTLIBRARIES = \
	libinput-sources.la \
	$(NULL)

libinput_sources_la_CFLAGS = \
	$(INPUT_SOURCES_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(srcdir)/../../gnome-flashback \
	-I$(top_builddir)/gnome-flashback \
	-I$(top_builddir)/gnome-flashback/libinput-sources \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	$(NULL)

libinput_sources_la_SOURCES = \
	gf-candidate-popup.c \
	gf-candidate-popup.h \
	gf-ibus-manager.c \
	gf-ibus-manager.h \
	gf-input-sources.c \
	gf-input-sources.h \
	gf-input-source-manager.c \
	gf-input-source-manager.h \
	gf-input-source-settings.c \
	gf-input-source-settings.h \
	gf-keyboard-manager.c \
	gf-keyboard-manager.h \
	$(NULL)

libinput_sources_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libinput_sources_la_LIBADD = \
	$(top_builddir)/gnome-flashback/libcommon/libcommon.la \
	$(INPUT_SOURCES_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
