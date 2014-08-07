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

GETTEXT_PACKAGE=gnome-flashback
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define the gettext package to be used])

IT_PROG_INTLTOOL([0.40.6])
AC_PROG_CC

GTK_REQUIRED=3.10.0

PKG_CHECK_MODULES(GNOME_FLASHBACK, gtk+-3.0 >= $GTK_REQUIRED)
AC_SUBST(GNOME_FLASHBACK_CFLAGS)
AC_SUBST(GNOME_FLASHBACK_LIBS)

AC_CONFIG_FILES([
Makefile
data/Makefile
gnome-flashback/Makefile
po/Makefile.in
])

AC_OUTPUT

echo "
    gnome-flashback $VERSION
    `echo gnome-flashback $VERSION | sed "s/./=/g"`

    prefix:                        ${prefix}
    sysconfdir:                    ${sysconfdir}
    source code location:          ${srcdir}
    compiler:                      ${CC}
"
