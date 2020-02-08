NULL =

noinst_LTLIBRARIES = \
	libinput-sources.la \
	$(NULL)

libinput_sources_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"input-sources\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-I$(top_builddir)/gnome-flashback/libinput-sources \
	-I$(top_srcdir) \
	-I$(top_srcdir)/gnome-flashback \
	$(NULL)

libinput_sources_la_CFLAGS = \
	$(INPUT_SOURCES_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libinput_sources_la_SOURCES = \
	gf-candidate-box.c \
	gf-candidate-box.h \
	gf-candidate-area.c \
	gf-candidate-area.h \
	gf-candidate-popup.c \
	gf-candidate-popup.h \
	gf-ibus-manager.c \
	gf-ibus-manager.h \
	gf-input-source-ibus.c \
	gf-input-source-ibus.h \
	gf-input-source-xkb.c \
	gf-input-source-xkb.h \
	gf-input-source.c \
	gf-input-source.h \
	gf-input-sources-button.c \
	gf-input-sources-button.h \
	gf-input-sources.c \
	gf-input-sources.h \
	gf-input-source-manager.c \
	gf-input-source-manager.h \
	gf-input-source-popup.c \
	gf-input-source-popup.h \
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
	$(top_builddir)/dbus/libdbus.la \
	$(top_builddir)/gnome-flashback/libcommon/libcommon.la \
	$(INPUT_SOURCES_LIBS) \
	$(LIBM) \
	$(NULL)

-include $(top_srcdir)/git.mk
