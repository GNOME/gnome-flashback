NULL =

noinst_LTLIBRARIES = \
	libroot-background.la \
	$(NULL)

libroot_background_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"root-background\" \
	-DG_LOG_USE_STRUCTURED=1 \
	$(NULL)

libroot_background_la_CFLAGS = \
	$(ROOT_BACKGROUND_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libroot-background \
	-DGNOME_DESKTOP_USE_UNSTABLE_API \
	$(NULL)

libroot_background_la_SOURCES = \
	gf-root-background.c \
	gf-root-background.h \
	$(NULL)

libroot_background_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libroot_background_la_LIBADD = \
	$(DROOT_BACKGROUND_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
