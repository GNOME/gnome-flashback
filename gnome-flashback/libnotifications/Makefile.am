NULL =

noinst_LTLIBRARIES = \
	libnotifications.la \
	$(NULL)

libnotifications_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"notifications\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir)/gnome-flashback \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libnotifications_la_CFLAGS = \
	$(NOTIFICATIONS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libnotifications_la_SOURCES = \
	gf-bubble.c \
	gf-bubble.h \
	gf-notifications.c \
	gf-notifications.h \
	nd-daemon.c \
	nd-daemon.h \
	nd-notification.c \
	nd-notification.h \
	nd-notification-box.c \
	nd-notification-box.h \
	nd-queue.c \
	nd-queue.h \
	nd-stack.c \
	nd-stack.h \
	$(NULL)

libnotifications_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libnotifications_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(top_builddir)/gnome-flashback/libcommon/libcommon.la \
	$(NOTIFICATIONS_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
