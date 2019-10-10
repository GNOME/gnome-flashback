NULL =

noinst_LTLIBRARIES = \
	libclipboard.la \
	$(NULL)

libclipboard_la_CPPFLAGS = \
	-I$(top_builddir)/gnome-flashback/libclipboard \
	$(NULL)

libclipboard_la_CFLAGS = \
	$(CLIPBOARD_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libclipboard_la_SOURCES = \
	gf-clipboard.c \
	gf-clipboard.h \
	gsd-clipboard-manager.c \
	gsd-clipboard-manager.h \
	list.c \
	list.h \
	xutils.c \
	xutils.h \
	$(NULL)

libclipboard_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libclipboard_la_LIBADD = \
	$(CLIPBOARD_LIBS) \
	$(LIBM) \
	$(NULL)

-include $(top_srcdir)/git.mk
