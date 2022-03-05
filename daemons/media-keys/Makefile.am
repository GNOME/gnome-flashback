NULL =

libexec_PROGRAMS = \
	gnome-flashback-media-keys \
	$(NULL)

gnome_flashback_media_keys_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gnome-flashback-media-keys\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(NULL)

gnome_flashback_media_keys_CFLAGS = \
	$(MEDIA_KEYS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

gnome_flashback_media_keys_SOURCES = \
	gf-media-keys-main.c \
	gf-media-keys.c \
	gf-media-keys.h \
	gsd-media-keys-manager.c \
	gsd-media-keys-manager.h \
	gsd-screenshot-utils.c \
	gsd-screenshot-utils.h \
	media-keys.h \
	shell-action-modes.h \
	shortcuts-list.h \
	$(NULL)

gnome_flashback_media_keys_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gnome_flashback_media_keys_LDADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(MEDIA_KEYS_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
