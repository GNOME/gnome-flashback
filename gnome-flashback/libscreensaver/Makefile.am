NULL =

noinst_LTLIBRARIES = \
	libscreensaver.la \
	$(NULL)

libscreensaver_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"screensaver\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-I$(top_builddir)/gnome-flashback/libscreensaver \
	$(NULL)

libscreensaver_la_CFLAGS = \
	$(SCREENSAVER_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libscreensaver_la_SOURCES = \
	gf-auth.c \
	gf-auth.h \
	gf-grab.c \
	gf-grab.h \
	gf-info-bar.c \
	gf-info-bar.h \
	gf-listener.c \
	gf-listener.h \
	gf-panel.c \
	gf-panel.h \
	gf-prefs.c \
	gf-prefs.h \
	gf-screensaver.c \
	gf-screensaver.h \
	gf-screensaver-utils.c \
	gf-screensaver-utils.h \
	gf-watcher.c \
	gf-watcher.h \
	$(BUILT_SOURCES) \
	$(NULL)

libscreensaver_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libscreensaver_la_LIBADD = \
	$(SCREENSAVER_LIBS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gf-auth.h \
	$(NULL)

gf-screensaver-enum-types.c: gf-screensaver-enum-types.c.in gf-screensaver-enum-types.h $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-screensaver-enum-types.c.in $(ENUM_TYPES) > \
		gf-screensaver-enum-types.c.tmp && mv gf-screensaver-enum-types.c.tmp gf-screensaver-enum-types.c

gf-screensaver-enum-types.h: gf-screensaver-enum-types.h.in $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-screensaver-enum-types.h.in $(ENUM_TYPES) > \
		gf-screensaver-enum-types.h.tmp && mv gf-screensaver-enum-types.h.tmp gf-screensaver-enum-types.h

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

gf-dm-seat-gen.h:
gf-dm-seat-gen.c: org.freedesktop.DisplayManager.Seat.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-dm-seat-gen \
		$(srcdir)/org.freedesktop.DisplayManager.Seat.xml

gf-login-manager-gen.h:
gf-login-manager-gen.c: org.freedesktop.login1.Manager.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-login-manager-gen \
		$(srcdir)/org.freedesktop.login1.Manager.xml

gf-login-seat-gen.h:
gf-login-seat-gen.c: org.freedesktop.login1.Seat.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-login-seat-gen \
		$(srcdir)/org.freedesktop.login1.Seat.xml

gf-login-session-gen.h:
gf-login-session-gen.c: org.freedesktop.login1.Session.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-login-session-gen \
		$(srcdir)/org.freedesktop.login1.Session.xml

gf-screensaver-gen.h:
gf-screensaver-gen.c: org.gnome.ScreenSaver.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-screensaver-gen \
		$(srcdir)/org.gnome.ScreenSaver.xml

gf-sm-presence-gen.h:
gf-sm-presence-gen.c: org.gnome.SessionManager.Presence.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-sm-presence-gen \
		$(srcdir)/org.gnome.SessionManager.Presence.xml

BUILT_SOURCES = \
	gf-accounts-gen.c \
	gf-accounts-gen.h \
	gf-accounts-user-gen.c \
	gf-accounts-user-gen.h \
	gf-dm-seat-gen.c \
	gf-dm-seat-gen.h \
	gf-login-manager-gen.c \
	gf-login-manager-gen.h \
	gf-login-seat-gen.c \
	gf-login-seat-gen.h \
	gf-login-session-gen.c \
	gf-login-session-gen.h \
	gf-screensaver-enum-types.c \
	gf-screensaver-enum-types.h \
	gf-screensaver-gen.c \
	gf-screensaver-gen.h \
	gf-sm-presence-gen.c \
	gf-sm-presence-gen.h \
	$(NULL)

EXTRA_DIST = \
	gf-screensaver-enum-types.c.in \
	gf-screensaver-enum-types.h.in \
	org.freedesktop.Accounts.xml \
	org.freedesktop.Accounts.User.xml \
	org.freedesktop.DisplayManager.Seat.xml \
	org.freedesktop.login1.Manager.xml \
	org.freedesktop.login1.Seat.xml \
	org.freedesktop.login1.Session.xml \
	org.gnome.ScreenSaver.xml \
	org.gnome.SessionManager.Presence.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
