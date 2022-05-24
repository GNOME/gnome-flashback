NULL =

libexec_PROGRAMS = \
	gnome-flashback-idle-monitor \
	$(NULL)

gnome_flashback_idle_monitor_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gnome-flashback-idle-monitor\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_builddir)/daemons/idle-monitor \
	-I$(top_srcdir) \
	$(NULL)

gnome_flashback_idle_monitor_CFLAGS = \
	$(IDLE_MONITOR_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

gnome_flashback_idle_monitor_SOURCES = \
	flashback-idle-monitor.c \
	flashback-idle-monitor.h \
	gf-idle-monitor-main.c \
	meta-dbus-idle-monitor.c \
	meta-dbus-idle-monitor.h \
	meta-idle-monitor.c \
	meta-idle-monitor.h \
	$(NULL)

gnome_flashback_idle_monitor_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gnome_flashback_idle_monitor_LDADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(IDLE_MONITOR_LIBS) \
	$(NULL)

meta-dbus-idle-monitor.h:
meta-dbus-idle-monitor.c: org.gnome.Mutter.IdleMonitor.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace MetaDBus \
		--generate-c-code meta-dbus-idle-monitor \
		$(srcdir)/org.gnome.Mutter.IdleMonitor.xml

BUILT_SOURCES = \
	meta-dbus-idle-monitor.h \
	meta-dbus-idle-monitor.c \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Mutter.IdleMonitor.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
