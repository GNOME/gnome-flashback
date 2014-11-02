noinst_LTLIBRARIES = \
	libkey-grabber.la

AM_CPPFLAGS = \
	$(KEY_GRABBER_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libkey-grabber

libkey_grabber_la_SOURCES = \
	dbus-key-grabber.c \
	dbus-key-grabber.h \
	flashback-key-grabber.c \
	flashback-key-grabber.h \
	flashback-key-bindings.c \
	flashback-key-bindings.h

libkey_grabber_la_LIBADD = \
	$(KEY_GRABBER_LIBS)

dbus-key-grabber.h:
dbus-key-grabber.c: org.gnome.KeyGrabber.xml
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix org.gnome. \
		--c-namespace DBus \
		--generate-c-code dbus-key-grabber \
		--c-generate-object-manager \
		$(srcdir)/org.gnome.KeyGrabber.xml

BUILT_SOURCES = \
	dbus-key-grabber.c \
	dbus-key-grabber.h

EXTRA_DIST = \
	org.gnome.KeyGrabber.xml

CLEANFILES = \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
