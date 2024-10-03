NULL =

noinst_LTLIBRARIES = \
	libbackends.la \
	$(NULL)

libbackends_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gf-backends\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libbackends_la_CFLAGS = \
	$(BACKENDS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libbackends_la_SOURCES = \
	gf-backend-private.h \
	gf-backend-x11-cm-private.h \
	gf-backend-x11-cm.c \
	gf-backend-x11-private.h \
	gf-backend-x11.c \
	gf-backend.c \
	gf-backend.h \
	gf-crtc-mode-info-private.h \
	gf-crtc-mode-info.c \
	gf-crtc-mode-private.h \
	gf-crtc-mode.c \
	gf-crtc-private.h \
	gf-crtc-xrandr-private.h \
	gf-crtc-xrandr.c \
	gf-crtc.c \
	gf-direction.h \
	gf-display-config-shared.h \
	gf-edid-parse.c \
	gf-edid-private.h \
	gf-gpu-private.h \
	gf-gpu-xrandr-private.h \
	gf-gpu-xrandr.c \
	gf-gpu.c \
	gf-logical-monitor-config-private.h \
	gf-logical-monitor-config.c \
	gf-logical-monitor-private.h \
	gf-logical-monitor.c \
	gf-monitor-config-manager-private.h \
	gf-monitor-config-manager.c \
	gf-monitor-config-private.h \
	gf-monitor-config-store-private.h \
	gf-monitor-config-store.c \
	gf-monitor-config-utils.c \
	gf-monitor-config-utils.h \
	gf-monitor-config.c \
	gf-monitor-manager-enums-private.h \
	gf-monitor-manager-kms-private.h \
	gf-monitor-manager-kms.c \
	gf-monitor-manager-private.h \
	gf-monitor-manager-types-private.h \
	gf-monitor-manager-xrandr-private.h \
	gf-monitor-manager-xrandr.c \
	gf-monitor-manager.c \
	gf-monitor-manager.h \
	gf-monitor-normal-private.h \
	gf-monitor-normal.c \
	gf-monitor-private.h \
	gf-monitor-spec-private.h \
	gf-monitor-spec.c \
	gf-monitor-tiled-private.h \
	gf-monitor-tiled.c \
	gf-monitor-transform.c \
	gf-monitor-transform.h \
	gf-monitor.c \
	gf-monitors-config-private.h \
	gf-monitors-config.c \
	gf-orientation-manager-private.h \
	gf-orientation-manager.c \
	gf-output-info-private.h \
	gf-output-info.c \
	gf-output-private.h \
	gf-output-xrandr-private.h \
	gf-output-xrandr.c \
	gf-output.c \
	gf-rectangle-private.h \
	gf-rectangle.c \
	gf-rectangle.h \
	gf-settings-private.h \
	gf-settings.c \
	gf-settings.h \
	$(BUILT_SOURCES) \
	$(NULL)

libbackends_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libbackends_la_LIBADD = \
	$(BACKENDS_LIBS) \
	$(LIBM) \
	$(NULL)

gf-dbus-display-config.h:
gf-dbus-display-config.c: org.gnome.Mutter.DisplayConfig.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace GfDBus \
		--generate-c-code gf-dbus-display-config \
		--interface-prefix org.gnome.Mutter \
		$(srcdir)/org.gnome.Mutter.DisplayConfig.xml

BUILT_SOURCES = \
	gf-dbus-display-config.c \
	gf-dbus-display-config.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Mutter.DisplayConfig.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
