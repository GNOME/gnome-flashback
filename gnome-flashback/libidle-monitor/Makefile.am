NULL =

noinst_LTLIBRARIES = \
	libidle-monitor.la \
	$(NULL)

libidle_monitor_la_CFLAGS = \
	$(IDLE_MONITOR_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libidle-monitor \
	$(NULL)

libidle_monitor_la_SOURCES = \
	flashback-idle-monitor.c \
	flashback-idle-monitor.h \
	meta-dbus-idle-monitor.c \
	meta-dbus-idle-monitor.h \
	meta-backend.c \
	meta-backend.h \
	meta-idle-monitor.c \
	meta-idle-monitor.h \
	meta-idle-monitor-xsync.c \
	meta-idle-monitor-xsync.h \
	$(NULL)

libidle_monitor_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libidle_monitor_la_LIBADD = \
	$(IDLE_MONITOR_LIBS) \
	$(NULL)

meta-dbus-idle-monitor.h:
meta-dbus-idle-monitor.c: org.gnome.Mutter.IdleMonitor.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace MetaDBus \
		--generate-c-code meta-dbus-idle-monitor \
		--c-generate-object-manager \
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
