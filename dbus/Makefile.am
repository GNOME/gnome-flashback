NULL =

noinst_LTLIBRARIES = \
	libdbus.la \
	$(NULL)

libdbus_la_CPPFLAGS = \
	$(AM_CPPCFLAGS) \
	$(NULL)

libdbus_la_CFLAGS = \
	$(DBUS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libdbus_la_SOURCES = \
	$(BUILT_SOURCES) \
	$(NULL)

libdbus_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdbus_la_LIBADD = \
	$(DBUS_LIBS) \
	$(NULL)

gf-accounts-gen.h:
gf-accounts-gen.c: org.freedesktop.Accounts.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-accounts-gen \
		$(srcdir)/org.freedesktop.Accounts.xml

gf-accounts-user-gen.h:
gf-accounts-user-gen.c: org.freedesktop.Accounts.User.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-accounts-user-gen \
		$(srcdir)/org.freedesktop.Accounts.User.xml

gf-audio-device-selection-gen.h:
gf-audio-device-selection-gen.c: org.gnome.Shell.AudioDeviceSelection.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-audio-device-selection-gen \
		$(srcdir)/org.gnome.Shell.AudioDeviceSelection.xml

gf-dbus-x11.h:
gf-dbus-x11.c: org.gnome.Mutter.X11.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--c-namespace GfDBus \
		--generate-c-code gf-dbus-x11 \
		--interface-prefix org.gnome.Mutter \
		$(srcdir)/org.gnome.Mutter.X11.xml

gf-dm-seat-gen.h:
gf-dm-seat-gen.c: org.freedesktop.DisplayManager.Seat.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-dm-seat-gen \
		$(srcdir)/org.freedesktop.DisplayManager.Seat.xml

gf-end-session-dialog-gen.h:
gf-end-session-dialog-gen.c: org.gnome.SessionManager.EndSessionDialog.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-end-session-dialog-gen \
		$(srcdir)/org.gnome.SessionManager.EndSessionDialog.xml

gf-fd-application-gen.h:
gf-fd-application-gen.c: org.freedesktop.Application.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-fd-application-gen \
		$(srcdir)/org.freedesktop.Application.xml

gf-fd-dbus-gen.h:
gf-fd-dbus-gen.c: org.freedesktop.DBus.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-fd-dbus-gen \
		$(srcdir)/org.freedesktop.DBus.xml

gf-fd-notifications-gen.h:
gf-fd-notifications-gen.c: org.freedesktop.Notifications.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-fd-notifications-gen \
		$(srcdir)/org.freedesktop.Notifications.xml

gf-file-manager-gen.h:
gf-file-manager-gen.c: org.freedesktop.FileManager1.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-file-manager-gen \
		$(srcdir)/org.freedesktop.FileManager1.xml

gf-gtk-notifications-gen.h:
gf-gtk-notifications-gen.c: org.gtk.Notifications.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-gtk-notifications-gen \
		$(srcdir)/org.gtk.Notifications.xml

gf-input-sources-gen.h:
gf-input-sources-gen.c: org.gnome.Flashback.InputSources.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-input-sources-gen \
		$(srcdir)/org.gnome.Flashback.InputSources.xml

gf-login-manager-gen.h:
gf-login-manager-gen.c: org.freedesktop.login1.Manager.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-login-manager-gen \
		$(srcdir)/org.freedesktop.login1.Manager.xml

gf-login-session-gen.h:
gf-login-session-gen.c: org.freedesktop.login1.Session.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-login-session-gen \
		$(srcdir)/org.freedesktop.login1.Session.xml

gf-nautilus2-gen.h:
gf-nautilus2-gen.c: org.gnome.Nautilus.FileOperations2.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-nautilus2-gen \
		$(srcdir)/org.gnome.Nautilus.FileOperations2.xml

gf-upower-device-gen.h:
gf-upower-device-gen.c: org.freedesktop.UPower.Device.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-upower-device-gen \
		$(srcdir)/org.freedesktop.UPower.Device.xml

gf-screencast-gen.h:
gf-screencast-gen.c: org.gnome.Shell.Screencast.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-screencast-gen \
		$(srcdir)/org.gnome.Shell.Screencast.xml

gf-screensaver-gen.h:
gf-screensaver-gen.c: org.gnome.ScreenSaver.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-screensaver-gen \
		$(srcdir)/org.gnome.ScreenSaver.xml

gf-screenshot-gen.h:
gf-screenshot-gen.c: org.gnome.Shell.Screenshot.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-screenshot-gen \
		$(srcdir)/org.gnome.Shell.Screenshot.xml

gf-sd-rfkill-gen.h:
gf-sd-rfkill-gen.c: org.gnome.SettingsDaemon.Rfkill.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-sd-rfkill-gen \
		$(srcdir)/org.gnome.SettingsDaemon.Rfkill.xml

gf-session-manager-gen.h:
gf-session-manager-gen.c: org.gnome.SessionManager.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-session-manager-gen \
		$(srcdir)/org.gnome.SessionManager.xml

gf-shell-gen.h:
gf-shell-gen.c: org.gnome.Shell.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-shell-gen \
		$(srcdir)/org.gnome.Shell.xml

gf-shell-introspect-gen.h:
gf-shell-introspect-gen.c: org.gnome.Shell.Introspect.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-shell-introspect-gen \
		$(srcdir)/org.gnome.Shell.Introspect.xml

gf-sm-client-private-gen.h:
gf-sm-client-private-gen.c: org.gnome.SessionManager.ClientPrivate.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-sm-client-private-gen \
		$(srcdir)/org.gnome.SessionManager.ClientPrivate.xml

gf-sm-presence-gen.h:
gf-sm-presence-gen.c: org.gnome.SessionManager.Presence.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-sm-presence-gen \
		$(srcdir)/org.gnome.SessionManager.Presence.xml

gf-sn-watcher-v0-gen.h:
gf-sn-watcher-v0-gen.c: org.kde.StatusNotifierWatcher.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-sn-watcher-v0-gen \
		$(srcdir)/org.kde.StatusNotifierWatcher.xml

BUILT_SOURCES = \
	gf-accounts-gen.c \
	gf-accounts-gen.h \
	gf-accounts-user-gen.c \
	gf-accounts-user-gen.h \
	gf-audio-device-selection-gen.c \
	gf-audio-device-selection-gen.h \
	gf-dbus-x11.c \
	gf-dbus-x11.h \
	gf-dm-seat-gen.c \
	gf-dm-seat-gen.h \
	gf-end-session-dialog-gen.c \
	gf-end-session-dialog-gen.h \
	gf-fd-application-gen.c \
	gf-fd-application-gen.h \
	gf-fd-dbus-gen.c \
	gf-fd-dbus-gen.h \
	gf-fd-notifications-gen.c \
	gf-fd-notifications-gen.h \
	gf-file-manager-gen.c \
	gf-file-manager-gen.h \
	gf-gtk-notifications-gen.c \
	gf-gtk-notifications-gen.h \
	gf-input-sources-gen.c \
	gf-input-sources-gen.h \
	gf-login-manager-gen.c \
	gf-login-manager-gen.h \
	gf-login-session-gen.c \
	gf-login-session-gen.h \
	gf-nautilus2-gen.c \
	gf-nautilus2-gen.h \
	gf-upower-device-gen.c \
	gf-upower-device-gen.h \
	gf-screencast-gen.c \
	gf-screencast-gen.h \
	gf-screensaver-gen.c \
	gf-screensaver-gen.h \
	gf-screenshot-gen.c \
	gf-screenshot-gen.h \
	gf-sd-rfkill-gen.c \
	gf-sd-rfkill-gen.h \
	gf-session-manager-gen.c \
	gf-session-manager-gen.h \
	gf-shell-gen.c \
	gf-shell-gen.h \
	gf-shell-introspect-gen.c \
	gf-shell-introspect-gen.h \
	gf-sm-client-private-gen.c \
	gf-sm-client-private-gen.h \
	gf-sm-presence-gen.c \
	gf-sm-presence-gen.h \
	gf-sn-watcher-v0-gen.c \
	gf-sn-watcher-v0-gen.h \
	$(NULL)

EXTRA_DIST = \
	org.freedesktop.Accounts.xml \
	org.freedesktop.Accounts.User.xml \
	org.freedesktop.Application.xml \
	org.freedesktop.DBus.xml \
	org.freedesktop.DisplayManager.Seat.xml \
	org.freedesktop.FileManager1.xml \
	org.freedesktop.login1.Manager.xml \
	org.freedesktop.login1.Session.xml \
	org.freedesktop.Notifications.xml \
	org.freedesktop.UPower.Device.xml \
	org.gnome.Flashback.InputSources.xml \
	org.gnome.Mutter.X11.xml \
	org.gnome.Nautilus.FileOperations2.xml \
	org.gnome.ScreenSaver.xml \
	org.gnome.SessionManager.ClientPrivate.xml \
	org.gnome.SessionManager.EndSessionDialog.xml \
	org.gnome.SessionManager.Presence.xml \
	org.gnome.SessionManager.xml \
	org.gnome.SettingsDaemon.Rfkill.xml \
	org.gnome.Shell.AudioDeviceSelection.xml \
	org.gnome.Shell.Introspect.xml \
	org.gnome.Shell.Screencast.xml \
	org.gnome.Shell.Screenshot.xml \
	org.gnome.Shell.xml \
	org.gtk.Notifications.xml \
	org.kde.StatusNotifierWatcher.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
