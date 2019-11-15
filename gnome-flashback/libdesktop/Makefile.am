NULL =

noinst_LTLIBRARIES = \
	libdesktop.la \
	$(NULL)

libdesktop_la_CPPFLAGS = \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	-I$(top_builddir)/gnome-flashback/libdesktop \
	$(NULL)

libdesktop_la_CFLAGS = \
	$(DESKTOP_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libdesktop_la_SOURCES = \
	gf-background.c \
	gf-background.h \
	gf-create-folder-dialog.c \
	gf-create-folder-dialog.h \
	gf-desktop-enums.h \
	gf-desktop-window.c \
	gf-desktop-window.h \
	gf-desktop.c \
	gf-desktop.h \
	gf-dummy-icon.c \
	gf-dummy-icon.h \
	gf-home-icon.c \
	gf-home-icon.h \
	gf-trash-icon.c \
	gf-trash-icon.h \
	gf-icon-view.c \
	gf-icon-view.h \
	gf-icon.c \
	gf-icon.h \
	gf-monitor-view.c \
	gf-monitor-view.h \
	gf-utils.c \
	gf-utils.h \
	$(BUILT_SOURCES) \
	$(NULL)

libdesktop_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdesktop_la_LIBADD = \
	$(DESKTOP_LIBS) \
	$(NULL)

ENUM_TYPES = \
	$(srcdir)/gf-desktop-enums.h \
	$(NULL)

gf-desktop-enum-types.c: gf-desktop-enum-types.c.in gf-desktop-enum-types.h $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-desktop-enum-types.c.in $(ENUM_TYPES) > \
		gf-desktop-enum-types.c.tmp && mv gf-desktop-enum-types.c.tmp gf-desktop-enum-types.c

gf-desktop-enum-types.h: gf-desktop-enum-types.h.in $(ENUM_TYPES)
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gf-desktop-enum-types.h.in $(ENUM_TYPES) > \
		gf-desktop-enum-types.h.tmp && mv gf-desktop-enum-types.h.tmp gf-desktop-enum-types.h

gf-file-manager-gen.h:
gf-file-manager-gen.c: org.freedesktop.FileManager1.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-file-manager-gen \
		$(srcdir)/org.freedesktop.FileManager1.xml

gf-nautilus-gen.h:
gf-nautilus-gen.c: org.gnome.Nautilus.FileOperations.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-nautilus-gen \
		$(srcdir)/org.gnome.Nautilus.FileOperations.xml

BUILT_SOURCES = \
	gf-desktop-enum-types.c \
	gf-desktop-enum-types.h \
	gf-file-manager-gen.c \
	gf-file-manager-gen.h \
	gf-nautilus-gen.c \
	gf-nautilus-gen.h \
	$(NULL)

EXTRA_DIST = \
	gf-desktop-enum-types.c.in \
	gf-desktop-enum-types.h.in \
	org.freedesktop.FileManager1.xml \
	org.gnome.Nautilus.FileOperations.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
