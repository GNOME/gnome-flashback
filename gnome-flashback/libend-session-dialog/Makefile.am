NULL =

noinst_LTLIBRARIES = \
	libend-session-dialog.la \
	$(NULL)

libend_session_dialog_la_CFLAGS = \
	$(END_SESSION_DIALOG_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libend-session-dialog \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	$(NULL)

libend_session_dialog_la_SOURCES = \
	dbus-end-session-dialog.c \
	dbus-end-session-dialog.h \
	flashback-inhibit-dialog.c \
	flashback-inhibit-dialog.h \
	gf-end-session-dialog.c \
	gf-end-session-dialog.h \
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
	flashback-inhibit-dialog.ui \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
