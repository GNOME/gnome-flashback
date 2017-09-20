NULL =

noinst_LTLIBRARIES = \
	libdisplay-config.la
	$(NULL)

libdisplay_config_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"display-config\" \
	-DG_LOG_USE_STRUCTURED=1 \
	$(NULL)

libdisplay_config_la_CFLAGS = \
	$(DISPLAY_CONFIG_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libdisplay-config \
	$(NULL)

libdisplay_config_la_SOURCES = \
	edid-parse.c \
	edid.h \
	meta-display-config-shared.h \
	flashback-confirm-dialog.c \
	flashback-confirm-dialog.h \
	flashback-display-config.c \
	flashback-display-config.h \
	flashback-monitor-manager.c \
	flashback-monitor-manager.h \
	$(NULL)

libdisplay_config_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdisplay_config_la_LIBADD = \
	$(DISPLAY_CONFIG_LIBS) \
	$(NULL)

EXTRA_DIST = \
	flashback-confirm-dialog.ui \
	$(NULL)

-include $(top_srcdir)/git.mk
