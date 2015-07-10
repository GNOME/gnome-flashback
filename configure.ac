m4_define([gnome_flashback_major_version], [3])
m4_define([gnome_flashback_minor_version], [17])
m4_define([gnome_flashback_micro_version], [2])

AC_INIT([gnome-flashback], [gnome_flashback_major_version.gnome_flashback_minor_version.gnome_flashback_micro_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-flashback])

AX_IS_RELEASE([git-directory])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar -Wno-portability])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([enable])

AX_CHECK_ENABLE_DEBUG([yes])

AC_PROG_CC

AX_COMPILER_FLAGS([WARN_CFLAGS], [WARN_LDFLAGS])

LT_INIT

AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

GETTEXT_PACKAGE=gnome-flashback
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define the gettext package to be used])
IT_PROG_INTLTOOL([0.40.6])

GLIB_GSETTINGS

GTK_REQUIRED=3.15.2
LIBGNOME_DESKTOP_REQUIRED=3.12.0
CANBERRA_REQUIRED=0.13
GLIB_REQUIRED=2.44.0
GSETTINGS_DESKTOP_SCHEMAS_REQUIRED=3.12.0

PKG_CHECK_MODULES(GNOME_FLASHBACK, gtk+-3.0 >= $GTK_REQUIRED dbus-glib-1)
AC_SUBST(GNOME_FLASHBACK_CFLAGS)
AC_SUBST(GNOME_FLASHBACK_LIBS)

PKG_CHECK_MODULES(AUTOMOUNT_MANAGER, gtk+-3.0 >= $GTK_REQUIRED gsettings-desktop-schemas >= $GSETTINGS_DESKTOP_SCHEMAS_REQUIRED)
AC_SUBST(AUTOMOUNT_MANAGER_CFLAGS)
AC_SUBST(AUTOMOUNT_MANAGER_LIBS)

PKG_CHECK_MODULES(DESKTOP_BACKGROUND, gtk+-3.0 >= $GTK_REQUIRED gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED)
AC_SUBST(DESKTOP_BACKGROUND_CFLAGS)
AC_SUBST(DESKTOP_BACKGROUND_LIBS)

PKG_CHECK_MODULES(DISPLAY_CONFIG, gtk+-3.0 >= $GTK_REQUIRED glib-2.0 >= $GLIB_REQUIRED gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED upower-glib >= 0.99.0 x11 xext xrandr x11-xcb xcb-randr)
AC_SUBST(DISPLAY_CONFIG_CFLAGS)
AC_SUBST(DISPLAY_CONFIG_LIBS)

PKG_CHECK_MODULES(END_SESSION_DIALOG, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(END_SESSION_DIALOG_CFLAGS)
AC_SUBST(END_SESSION_DIALOG_LIBS)

PKG_CHECK_MODULES(IDLE_MONITOR, gtk+-3.0 >= $GTK_REQUIRED x11 xext)
AC_SUBST(IDLE_MONITOR_CFLAGS)
AC_SUBST(IDLE_MONITOR_LIBS)

PKG_CHECK_MODULES(SCREENCAST, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(SCREENCAST_CFLAGS)
AC_SUBST(SCREENCAST_LIBS)

PKG_CHECK_MODULES(SCREENSHOT, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(SCREENSHOT_CFLAGS)
AC_SUBST(SCREENSHOT_LIBS)

PKG_CHECK_MODULES(SHELL, gtk+-3.0 >= $GTK_REQUIRED glib-2.0 >= $GLIB_REQUIRED gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED x11)
AC_SUBST(SHELL_CFLAGS)
AC_SUBST(SHELL_LIBS)

PKG_CHECK_MODULES(GVC, gobject-2.0 libpulse libpulse-mainloop-glib)
AM_CONDITIONAL(HAVE_INTROSPECTION, false)

PKG_CHECK_MODULES(SOUND_APPLET, gtk+-3.0 >= $GTK_REQUIRED libcanberra-gtk3 >= $CANBERRA_REQUIRED glib-2.0 >= $GLIB_REQUIRED)
AC_SUBST(SOUND_APPLET_CFLAGS)
AC_SUBST(SOUND_APPLET_LIBS)

PKG_CHECK_MODULES(WORKAROUNDS, glib-2.0 >= $GLIB_REQUIRED gtk+-3.0 >= $GTK_REQUIRED x11)
AC_SUBST(WORKAROUNDS_CFLAGS)
AC_SUBST(WORKAROUNDS_LIBS)

PKG_CHECK_EXISTS([xrandr >= 1.5.0], AC_DEFINE([HAVE_XRANDR15], [1], [Define if you have support for XRandR 1.5 or greater]))

AC_CONFIG_FILES([
Makefile
data/Makefile
gnome-flashback/Makefile
gnome-flashback/libautomount-manager/Makefile
gnome-flashback/libdesktop-background/Makefile
gnome-flashback/libdisplay-config/Makefile
gnome-flashback/libend-session-dialog/Makefile
gnome-flashback/libidle-monitor/Makefile
gnome-flashback/libscreencast/Makefile
gnome-flashback/libscreenshot/Makefile
gnome-flashback/libshell/Makefile
gnome-flashback/libsound-applet/Makefile
gnome-flashback/libsound-applet/gvc/Makefile
gnome-flashback/libworkarounds/Makefile
po/Makefile.in
])

AC_OUTPUT

echo "
    gnome-flashback $VERSION
    `echo gnome-flashback $VERSION | sed "s/./=/g"`

    prefix:                        ${prefix}
    exec_prefix:                   ${exec_prefix}

    sysconfdir:                    ${sysconfdir}
    libexecdir:                    ${libexecdir}

    source code location:          ${srcdir}
    compiler:                      ${CC}
"
