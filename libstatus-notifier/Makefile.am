NULL =

noinst_LTLIBRARIES = libstatus-notifier.la

libstatus_notifier_la_SOURCES = \
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
	$(NULL)

EXTRA_DIST = \
	com.canonical.dbusmenu.xml \
	org.kde.StatusNotifierItem.xml \
	org.kde.StatusNotifierWatcher.xml \
	$(NULL)

-include $(top_srcdir)/git.mk
