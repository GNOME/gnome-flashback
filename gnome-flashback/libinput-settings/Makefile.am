NULL =

noinst_LTLIBRARIES = \
	libinput-settings.la \
	$(NULL)

libinput_settings_la_CPPFLAGS = \
	-I$(top_builddir)/gnome-flashback/libinput-settings \
	-I$(top_srcdir)/gnome-flashback \
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
