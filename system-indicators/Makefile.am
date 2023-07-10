NULL =

system_indicators_libdir = $(GNOME_PANEL_MODULES_DIR)
system_indicators_lib_LTLIBRARIES = system_indicators.la

system_indicators_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"system-indicators\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DLOCALE_DIR=\"$(localedir)\" \
	-I$(top_srcdir)/gvc/gvc \
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
	si-desktop-menu-item.c \
	si-desktop-menu-item.h \
	si-indicator.c \
	si-indicator.h \
	si-input-sources.c \
	si-input-sources.h \
	si-menu-bar.c \
	si-menu-bar.h \
	si-module.c \
	si-power.c \
	si-power.h \
	si-volume.c \
	si-volume.h \
	$(NULL)

system_indicators_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(top_builddir)/gvc/libgvc.la \
	$(SYSTEM_INDICATORS_LIBS) \
	$(LIBM) \
	$(NULL)

system_indicators_la_LDFLAGS = \
	-module -avoid-version \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

if HAVE_GNOME_BLUETOOTH
system_indicators_la_SOURCES += \
	si-bluetooth.c \
	si-bluetooth.h \
	$(NULL)

system_indicators_la_CFLAGS += \
	-Wno-error=missing-include-dirs \
	$(GNOME_BLUETOOTH_CFLAGS) \
	$(NULL)

system_indicators_la_LIBADD += \
	$(GNOME_BLUETOOTH_LIBS) \
	$(NULL)
endif

-include $(top_srcdir)/git.mk
