m4_define([gnome_flashback_major_version], [3])
m4_define([gnome_flashback_minor_version], [10])
m4_define([gnome_flashback_micro_version], [0])

AC_INIT([gnome-flashback], [gnome_flashback_major_version.gnome_flashback_minor_version.gnome_flashback_micro_version],
        [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-flashback])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz tar-ustar -Wno-portability])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AM_MAINTAINER_MODE([enable])

AC_PROG_CC
LT_INIT

GNOME_COMPILE_WARNINGS([maximum])
GNOME_MAINTAINER_MODE_DEFINES

AC_PATH_PROG(GLIB_COMPILE_RESOURCES, glib-compile-resources)

GETTEXT_PACKAGE=gnome-flashback
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define the gettext package to be used])
IT_PROG_INTLTOOL([0.40.6])

GLIB_GSETTINGS

GTK_REQUIRED=3.10.0
LIBGNOME_DESKTOP_REQUIRED=3.8.0
CANBERRA_REQUIRED=0.13
GLIB_REQUIRED=2.39.91

PKG_CHECK_MODULES(GNOME_FLASHBACK, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(GNOME_FLASHBACK_CFLAGS)
AC_SUBST(GNOME_FLASHBACK_LIBS)

PKG_CHECK_MODULES(END_SESSION_DIALOG, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(END_SESSION_DIALOG_CFLAGS)
AC_SUBST(END_SESSION_DIALOG_LIBS)

PKG_CHECK_MODULES(DESKTOP_BACKGROUND, gtk+-3.0 >= $GTK_REQUIRED gnome-desktop-3.0 >= $LIBGNOME_DESKTOP_REQUIRED)
AC_SUBST(DESKTOP_BACKGROUND_CFLAGS)
AC_SUBST(DESKTOP_BACKGROUND_LIBS)

PKG_CHECK_MODULES(DISPLAY_CONFIG, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(DISPLAY_CONFIG_CFLAGS)
AC_SUBST(DISPLAY_CONFIG_LIBS)

PKG_CHECK_MODULES(IDLE_MONITOR, gtk+-3.0 >= $GTK_REQUIRED x11 xext)
AC_SUBST(IDLE_MONITOR_CFLAGS)
AC_SUBST(IDLE_MONITOR_LIBS)

PKG_CHECK_MODULES(GVC, gobject-2.0 libpulse libpulse-mainloop-glib)
AM_CONDITIONAL(HAVE_INTROSPECTION, false)

PKG_CHECK_MODULES(SOUND_APPLET, gtk+-3.0 >= $GTK_REQUIRED libcanberra-gtk3 >= $CANBERRA_REQUIRED glib-2.0 >= $GLIB_REQUIRED)
AC_SUBST(SOUND_APPLET_CFLAGS)
AC_SUBST(SOUND_APPLET_LIBS)

AC_CONFIG_FILES([
Makefile
gnome-flashback/Makefile
gnome-flashback/libdesktop-background/Makefile
gnome-flashback/libdisplay-config/Makefile
gnome-flashback/libend-session-dialog/Makefile
gnome-flashback/libidle-monitor/Makefile
gnome-flashback/libsound-applet/Makefile
gnome-flashback/libsound-applet/gvc/Makefile
po/Makefile.in
sessions/Makefile
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
