NULL =

noinst_LTLIBRARIES = \
	libend-session-dialog.la \
	$(NULL)

libend_session_dialog_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"end-session-dialog\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libend_session_dialog_la_CFLAGS = \
	$(END_SESSION_DIALOG_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libend_session_dialog_la_SOURCES = \
	gf-end-session-dialog.c \
	gf-end-session-dialog.h \
	gf-inhibit-dialog.c \
	gf-inhibit-dialog.h \
	$(NULL)

libend_session_dialog_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libend_session_dialog_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(END_SESSION_DIALOG_LIBS) \
	$(NULL)

EXTRA_DIST = \
	gf-inhibit-dialog.ui \
	$(NULL)

-include $(top_srcdir)/git.mk
