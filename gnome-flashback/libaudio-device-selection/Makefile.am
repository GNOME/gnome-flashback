NULL =

noinst_LTLIBRARIES = \
	libaudio-device-selection.la \
	$(NULL)

libaudio_device_selection_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"audio-device-selection\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

libaudio_device_selection_la_CFLAGS = \
	$(AUDIO_DEVICE_SELECTION_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libaudio_device_selection_la_SOURCES = \
	gf-audio-device-selection.c \
	gf-audio-device-selection.h \
	gf-audio-device-selection-dialog.c \
	gf-audio-device-selection-dialog.h \
	$(NULL)

libaudio_device_selection_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libaudio_device_selection_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(AUDIO_DEVICE_SELECTION_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
