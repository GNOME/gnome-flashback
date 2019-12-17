NULL =

noinst_LTLIBRARIES = \
	libstatus-notifier-watcher.la \
	$(NULL)

libstatus_notifier_watcher_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"status-notifier-watcher\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
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
	$(NULL)

libstatus_notifier_watcher_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libstatus_notifier_watcher_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(STATUS_NOTIFIER_WATCHER_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
