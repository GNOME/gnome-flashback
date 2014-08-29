noinst_LTLIBRARIES = \
	libidle-monitor.la

AM_CPPFLAGS = \
	$(IDLE_MONITOR_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libidle-monitor \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

libidle_monitor_la_SOURCES = \
	dbus-idle-monitor.c \
	dbus-idle-monitor.h \
	flashback-idle-monitor.c \
	flashback-idle-monitor.h

libidle_monitor_la_LIBADD = \
	$(IDLE_MONITOR_LIBS)

dbus-idle-monitor.h:
dbus-idle-monitor.c: org.gnome.Mutter.IdleMonitor.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome.Mutter \
		--c-namespace DBus \
		--generate-c-code dbus-idle-monitor \
		--c-generate-object-manager \
		$(srcdir)/org.gnome.Mutter.IdleMonitor.xml

BUILT_SOURCES = \
	dbus-idle-monitor.h \
	dbus-idle-monitor.c

EXTRA_DIST = \
	org.gnome.Mutter.IdleMonitor.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
