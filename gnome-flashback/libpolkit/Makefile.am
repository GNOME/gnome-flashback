NULL =

noinst_LTLIBRARIES = \
	libpolkit.la \
	$(NULL)

libpolkit_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"polkit\" \
	-DG_LOG_USE_STRUCTURED=1 \
	$(NULL)

libpolkit_la_CFLAGS = \
	$(POLKIT_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libpolkit \
	-DPOLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE \
	$(NULL)

libpolkit_la_SOURCES = \
  flashback-authenticator.c \
	flashback-authenticator.h \
	flashback-listener.c \
	flashback-listener.h \
	flashback-polkit.c \
	flashback-polkit.h \
	flashback-polkit-dialog.c \
	flashback-polkit-dialog.h \
	$(NULL)

libpolkit_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libpolkit_la_LIBADD = \
	$(POLKIT_LIBS) \
	$(NULL)

EXTRA_DIST = \
	flashback-polkit-dialog.ui \
	$(NULL)

-include $(top_srcdir)/git.mk
