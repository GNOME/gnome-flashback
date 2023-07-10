NULL =

libexec_PROGRAMS = \
	gnome-flashback-polkit \
	$(NULL)

gnome_flashback_polkit_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gnome-flashback-polkit\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-DPOLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE \
	-I$(top_builddir)/daemons/polkit \
	-I$(top_srcdir) \
	$(NULL)

gnome_flashback_polkit_CFLAGS = \
	$(POLKIT_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

gnome_flashback_polkit_SOURCES = \
  flashback-authenticator.c \
	flashback-authenticator.h \
	flashback-listener.c \
	flashback-listener.h \
	flashback-polkit.c \
	flashback-polkit.h \
	flashback-polkit-dialog.c \
	flashback-polkit-dialog.h \
	gf-polkit-main.c \
	$(BUILT_SOURCES) \
	$(NULL)

gnome_flashback_polkit_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gnome_flashback_polkit_LDADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(POLKIT_LIBS) \
	$(NULL)

gf-resources.c: polkit.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/polkit.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name gf $<

gf-resources.h: polkit.gresource.xml $(shell $(GLIB_COMPILE_RESOURCES) --sourcedir=$(srcdir) --generate-dependencies $(srcdir)/polkit.gresource.xml)
	$(AM_V_GEN)$(GLIB_COMPILE_RESOURCES) --target=$@ --sourcedir=$(srcdir) --generate --c-name gf $<

BUILT_SOURCES = \
	gf-resources.c \
	gf-resources.h \
	$(NULL)

EXTRA_DIST = \
	flashback-polkit-dialog.ui \
	polkit.gresource.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
