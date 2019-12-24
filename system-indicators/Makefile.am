NULL =

if WITH_SYSTEM_INDICATORS
system_indicators_libdir = $(GNOME_PANEL_MODULES_DIR)
system_indicators_lib_LTLIBRARIES = system_indicators.la

system_indicators_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"system-indicators\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DLOCALE_DIR=\"$(localedir)\" \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

system_indicators_la_CFLAGS = \
	$(SYSTEM_INDICATORS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

system_indicators_la_SOURCES = \
	si-applet.c \
	si-applet.h \
	si-bluetooth.c \
	si-bluetooth.h \
	si-indicator.c \
	si-indicator.h \
	si-input-source.c \
	si-input-source.h \
	si-menu-bar.c \
	si-menu-bar.h \
	si-module.c \
	$(NULL)

system_indicators_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(SYSTEM_INDICATORS_LIBS) \
	$(NULL)

system_indicators_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)
endif

-include $(top_srcdir)/git.mk
