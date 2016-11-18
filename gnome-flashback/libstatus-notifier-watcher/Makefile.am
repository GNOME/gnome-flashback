NULL =

noinst_LTLIBRARIES = \
	libstatus-notifier-watcher.la \
	$(NULL)

libstatus_notifier_watcher_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"status-notifier-watcher\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_builddir)/gnome-flashback/libstatus-notifier-watcher \
	$(NULL)

libstatus_notifier_watcher_la_CFLAGS = \
	$(STATUS_NOTIFIER_WATCHER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libstatus_notifier_watcher_la_SOURCES = \
	gf-sn-watcher-v0.c \
	gf-sn-watcher-v0.h \
	gf-status-notifier-watcher.c \
	gf-status-notifier-watcher.h \
	$(BUILT_SOURCES) \
	$(NULL)

libstatus_notifier_watcher_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libstatus_notifier_watcher_la_LIBADD = \
	$(STATUS_NOTIFIER_WATCHER_LIBS) \
	$(NULL)

gf-sn-watcher-v0-gen.h:
gf-sn-watcher-v0-gen.c: org.kde.StatusNotifierWatcher.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-sn-watcher-v0-gen \
		$(srcdir)/org.kde.StatusNotifierWatcher.xml

BUILT_SOURCES = \
	gf-sn-watcher-v0-gen.c \
	gf-sn-watcher-v0-gen.h \
	$(NULL)

EXTRA_DIST = \
	org.kde.StatusNotifierWatcher.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
