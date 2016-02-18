NULL =

noinst_LTLIBRARIES = \
	libnotifications.la \
	$(NULL)

libnotifications_la_CPPFLAGS = \
	-I$(top_builddir)/gnome-flashback/libnotifications \
	$(NULL)

libnotifications_la_CFLAGS = \
	$(NOTIFICATIONS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libnotifications_la_SOURCES = \
	gf-notifications.c \
	gf-notifications.h \
	$(NULL)

libnotifications_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libnotifications_la_LIBADD = \
	$(NOTIFICATIONS_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
