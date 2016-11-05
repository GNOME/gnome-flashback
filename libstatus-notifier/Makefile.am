NULL =

noinst_LTLIBRARIES = libstatus-notifier.la

libstatus_notifier_la_SOURCES = \
	sn-item.c \
	sn-item.h \
	sn-dbus-item.c \
	sn-dbus-item.h \
	sn-dbus-item-server.c \
	sn-dbus-item-server.h \
	sn-dbus-item-server-v0.c \
	sn-dbus-item-server-v0.h \
	sn-dbus-menu-server.c \
	sn-dbus-menu-server.h \
	sn-item-private.h \
	sn-watcher-private.h \
	$(BUILT_SOURCES) \
	$(NULL)

libstatus_notifier_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"libstatus-notifier\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libstatus_notifier_la_CFLAGS = \
	$(LIBSTATUS_NOTIFIER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libstatus_notifier_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libstatus_notifier_la_LIBADD = \
	$(LIBSTATUS_NOTIFIER_LIBS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/sn-item.h \
	$(NULL)

sn-enum-types.c: sn-enum-types.c.in sn-enum-types.h $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/sn-enum-types.c.in $(ENUM_TYPES) > \
		sn-enum-types.c.tmp && mv sn-enum-types.c.tmp sn-enum-types.c

sn-enum-types.h: sn-enum-types.h.in $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/sn-enum-types.h.in $(ENUM_TYPES) > \
		sn-enum-types.h.tmp && mv sn-enum-types.h.tmp sn-enum-types.h

sn-dbus-menu-gen.h:
sn-dbus-menu-gen.c: com.canonical.dbusmenu.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Sn \
		--generate-c-code sn-dbus-menu-gen \
		$(srcdir)/com.canonical.dbusmenu.xml

sn-item-v0-gen.h:
sn-item-v0-gen.c: org.kde.StatusNotifierItem.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Sn \
		--generate-c-code sn-item-v0-gen \
		$(srcdir)/org.kde.StatusNotifierItem.xml

sn-watcher-v0-gen.h:
sn-watcher-v0-gen.c: org.kde.StatusNotifierWatcher.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Sn \
		--generate-c-code sn-watcher-v0-gen \
		$(srcdir)/org.kde.StatusNotifierWatcher.xml

BUILT_SOURCES = \
	sn-dbus-menu-gen.c \
	sn-dbus-menu-gen.h \
	sn-item-v0-gen.c \
	sn-item-v0-gen.h \
	sn-watcher-v0-gen.c \
	sn-watcher-v0-gen.h \
	sn-enum-types.c \
	sn-enum-types.h \
	$(NULL)

EXTRA_DIST = \
	com.canonical.dbusmenu.xml \
	org.kde.StatusNotifierItem.xml \
	org.kde.StatusNotifierWatcher.xml \
	sn-enum-types.c.in \
	sn-enum-types.h.in \
	$(NULL)

-include $(top_srcdir)/git.mk
