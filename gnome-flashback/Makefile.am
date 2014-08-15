bin_PROGRAMS = \
	gnome-flashback

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_CFLAGS) \
	-I$(top_builddir)/gnome-flashback \
	-DGNOMELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	-DGNOME_DESKTOP_USE_UNSTABLE_API

gnome_flashback_SOURCES = \
	flashback-application.c \
	flashback-application.h \
	flashback-desktop-background.c \
	flashback-desktop-background.h \
	flashback-end-session-dialog-generated.c \
	flashback-end-session-dialog-generated.h \
	flashback-gsettings.h \
	flashback-inhibit-dialog.c \
	flashback-inhibit-dialog.h \
	flashback-main.c \
	flashback-resources.c \
	flashback-resources.h

gnome_flashback_LDADD = \
	$(GNOME_FLASHBACK_LIBS)

desktopdir       = $(datadir)/applications
desktop_in_files = gnome-flashback.desktop.in
desktop_DATA     = $(desktop_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

flashback-end-session-dialog-generated.h:
flashback-end-session-dialog-generated.c: org.gnome.SessionManager.EndSessionDialog.xml Makefile.am
	$(AM_V_GEN) gdbus-codegen \
		--interface-prefix=org.gnome.SessionManager \
		--generate-c-code flashback-end-session-dialog-generated \
		--c-namespace Flashback \
		org.gnome.SessionManager.EndSessionDialog.xml

flashback-resources.c: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<
flashback-resources.h: flashback.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/flashback.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name flashback $<

BUILT_SOURCES = \
	flashback-end-session-dialog-generated.h \
	flashback-end-session-dialog-generated.c \
	flashback-resources.c \
	flashback-resources.h

EXTRA_DIST = \
	$(desktop_in_files) \
	org.gnome.SessionManager.EndSessionDialog.xml \
	flashback.gresource.xml

CLEANFILES = \
	$(desktop_DATA) \
	$(BUILT_SOURCES)

-include $(top_srcdir)/git.mk
