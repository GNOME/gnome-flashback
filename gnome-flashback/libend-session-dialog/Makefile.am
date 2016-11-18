NULL =

noinst_LTLIBRARIES = \
	libend-session-dialog.la \
	$(NULL)

libend_session_dialog_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"end-session-dialog\" \
	-DG_LOG_USE_STRUCTURED=1 \
	$(NULL)

libend_session_dialog_la_CFLAGS = \
	$(END_SESSION_DIALOG_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libend-session-dialog \
	$(NULL)

libend_session_dialog_la_SOURCES = \
	dbus-end-session-dialog.c \
	dbus-end-session-dialog.h \
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
	$(END_SESSION_DIALOG_LIBS) \
	$(NULL)

dbus-end-session-dialog.h:
dbus-end-session-dialog.c: org.gnome.SessionManager.EndSessionDialog.xml Makefile.am
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix=org.gnome.SessionManager \
		--generate-c-code dbus-end-session-dialog \
		--c-namespace DBus \
		org.gnome.SessionManager.EndSessionDialog.xml

BUILT_SOURCES = \
	dbus-end-session-dialog.h \
	dbus-end-session-dialog.c \
	$(NULL)

EXTRA_DIST = \
	org.gnome.SessionManager.EndSessionDialog.xml \
	gf-inhibit-dialog.ui \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
