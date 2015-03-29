noinst_LTLIBRARIES = \
	libend-session-dialog.la

AM_CPPFLAGS = \
	$(END_SESSION_DIALOG_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libend-session-dailog \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

libend_session_dialog_la_SOURCES = \
	dbus-end-session-dialog.c \
	dbus-end-session-dialog.h \
	flashback-end-session-dialog.c \
	flashback-end-session-dialog.h \
	flashback-inhibit-dialog.c \
	flashback-inhibit-dialog.h

libend_session_dialog_la_LIBADD = \
	$(END_SESSION_DIALOG_LIBS)

dbus-end-session-dialog.h:
dbus-end-session-dialog.c: org.gnome.SessionManager.EndSessionDialog.xml Makefile.am
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix=org.gnome.SessionManager \
		--generate-c-code dbus-end-session-dialog \
		--c-namespace DBus \
		org.gnome.SessionManager.EndSessionDialog.xml

BUILT_SOURCES = \
	dbus-end-session-dialog.h \
	dbus-end-session-dialog.c

EXTRA_DIST = \
	org.gnome.SessionManager.EndSessionDialog.xml \
	flashback-inhibit-dialog.ui

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
