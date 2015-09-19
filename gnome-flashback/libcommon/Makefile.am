NULL =

noinst_LTLIBRARIES = \
	libcommon.la \
	$(NULL)

libcommon_la_CFLAGS = \
	$(COMMON_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libcommon \
	$(NULL)

libcommon_la_SOURCES = \
  gf-keybindings.c \
	gf-keybindings.h \
	$(NULL)

libcommon_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libcommon_la_LIBADD = \
	$(COMMON_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
