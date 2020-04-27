NULL =

libexec_PROGRAMS = \
	gnome-flashback-clipboard \
	$(NULL)

gnome_flashback_clipboard_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gnome-flashback-clipboard\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(NULL)

gnome_flashback_clipboard_CFLAGS = \
	$(CLIPBOARD_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

gnome_flashback_clipboard_SOURCES = \
	gf-clipboard-main.c \
	gf-clipboard.c \
	gf-clipboard.h \
	gsd-clipboard-manager.c \
	gsd-clipboard-manager.h \
	list.c \
	list.h \
	xutils.c \
	xutils.h \
	$(NULL)

gnome_flashback_clipboard_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

gnome_flashback_clipboard_LDADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(CLIPBOARD_LIBS) \
	$(LIBM) \
	$(NULL)

-include $(top_srcdir)/git.mk
