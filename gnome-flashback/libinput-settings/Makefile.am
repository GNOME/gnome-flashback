NULL =

noinst_LTLIBRARIES = \
	libinput-settings.la \
	$(NULL)

libinput_settings_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"input-settings\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_builddir)/backends \
	-I$(top_builddir)/gnome-flashback/libinput-settings \
	-I$(top_srcdir)/gnome-flashback \
	-I$(top_srcdir) \
	$(NULL)

libinput_settings_la_CFLAGS = \
	$(INPUT_SETTINGS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libinput_settings_la_SOURCES = \
	gf-input-settings.c \
	gf-input-settings.h \
	$(NULL)

libinput_settings_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libinput_settings_la_LIBADD = \
	$(INPUT_SETTINGS_LIBS) \
	$(LIBM) \
	$(NULL)

-include $(top_srcdir)/git.mk
