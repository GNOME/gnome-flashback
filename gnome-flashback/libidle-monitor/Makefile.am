noinst_LTLIBRARIES = \
	libidle-monitor.la

AM_CPPFLAGS = \
	$(IDLE_MONITOR_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libidle-monitor

libidle_monitor_la_SOURCES = \
	meta-dbus-idle-monitor.c \
	meta-dbus-idle-monitor.h \
	meta-backend.c \
	meta-backend.h \
	meta-idle-monitor.c \
	meta-idle-monitor.h \
	meta-idle-monitor-dbus.c \
	meta-idle-monitor-dbus.h \
	meta-idle-monitor-xsync.c \
	meta-idle-monitor-xsync.h

libidle_monitor_la_LIBADD = \
	$(IDLE_MONITOR_LIBS)

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
	meta-dbus-idle-monitor.c

EXTRA_DIST = \
	org.gnome.Mutter.IdleMonitor.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
