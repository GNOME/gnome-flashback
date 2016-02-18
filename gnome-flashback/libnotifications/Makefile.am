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
	nd-bubble.c \
	nd-bubble.h \
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
	$(BUILT_SOURCES) \
	$(NULL)

libnotifications_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libnotifications_la_LIBADD = \
	$(NOTIFICATIONS_LIBS) \
	$(NULL)

gf-fd-application-gen.h:
gf-fd-application-gen.c: org.freedesktop.Application.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.Application \
		--generate-c-code gf-fd-application-gen \
		--c-namespace Gf \
		--annotate "org.freedesktop.Application" \
		           "org.gtk.GDBus.C.Name" FdApplicationGen \
		$(srcdir)/org.freedesktop.Application.xml

gf-fd-dbus-gen.h:
gf-fd-dbus-gen.c: org.freedesktop.DBus.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.DBus \
		--generate-c-code gf-fd-dbus-gen \
		--c-namespace Gf \
		--annotate "org.freedesktop.DBus" \
		           "org.gtk.GDBus.C.Name" FdDBusGen \
		$(srcdir)/org.freedesktop.DBus.xml

gf-fd-notifications-gen.h:
gf-fd-notifications-gen.c: org.freedesktop.Notifications.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix org.freedesktop.Notifications \
		--generate-c-code gf-fd-notifications-gen \
		--c-namespace Gf \
		--annotate "org.freedesktop.Notifications" \
		           "org.gtk.GDBus.C.Name" FdNotificationsGen \
		$(srcdir)/org.freedesktop.Notifications.xml

gf-gtk-notifications-gen.h:
gf-gtk-notifications-gen.c: org.gtk.Notifications.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix org.gtk.Notifications \
		--generate-c-code gf-gtk-notifications-gen \
		--c-namespace Gf \
		--annotate "org.gtk.Notifications" \
		           "org.gtk.GDBus.C.Name" GtkNotificationsGen \
		$(srcdir)/org.gtk.Notifications.xml

BUILT_SOURCES = \
	gf-fd-application-gen.c \
	gf-fd-application-gen.h \
	gf-fd-dbus-gen.c \
	gf-fd-dbus-gen.h \
	gf-fd-notifications-gen.c \
	gf-fd-notifications-gen.h \
	gf-gtk-notifications-gen.c \
	gf-gtk-notifications-gen.h \
	$(NULL)

EXTRA_DIST = \
	org.freedesktop.Application.xml \
	org.freedesktop.DBus.xml \
	org.freedesktop.Notifications.xml \
	org.gtk.Notifications.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
