NULL =

noinst_LTLIBRARIES = \
	libgvc.la \
	$(NULL)

noinst_PROGRAMS = \
	test-audio-device-selection \
	$(NULL)

libgvc_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gvc\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir)/gvc/gvc \
	$(NULL)

libgvc_la_CFLAGS = \
	$(GVC_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libgvc_la_SOURCES = \
	gvc/gvc-channel-map-private.h \
	gvc/gvc-channel-map.c \
	gvc/gvc-channel-map.h \
	gvc/gvc-mixer-card-private.h \
	gvc/gvc-mixer-card.c \
	gvc/gvc-mixer-card.h \
	gvc/gvc-mixer-control-private.h \
	gvc/gvc-mixer-control.c \
	gvc/gvc-mixer-control.h \
	gvc/gvc-mixer-event-role.c \
	gvc/gvc-mixer-event-role.h \
	gvc/gvc-mixer-sink-input.c \
	gvc/gvc-mixer-sink-input.h \
	gvc/gvc-mixer-sink.c \
	gvc/gvc-mixer-sink.h \
	gvc/gvc-mixer-source-output.c \
	gvc/gvc-mixer-source-output.h \
	gvc/gvc-mixer-source.c \
	gvc/gvc-mixer-source.h \
	gvc/gvc-mixer-stream-private.h \
	gvc/gvc-mixer-stream.c \
	gvc/gvc-mixer-stream.h \
	gvc/gvc-mixer-ui-device.c \
	gvc/gvc-mixer-ui-device.h \
	gvc/gvc-pulseaudio-fake.h \
	$(NULL)

libgvc_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libgvc_la_LIBADD = \
	$(GVC_LIBS) \
	$(NULL)

test_audio_device_selection_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gvc\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir)/gvc/gvc \
	$(NULL)

test_audio_device_selection_CFLAGS = \
	$(GVC_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

test_audio_device_selection_SOURCES = \
	gvc/test-audio-device-selection.c \
	$(NULL)

test_audio_device_selection_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

test_audio_device_selection_LDADD = \
	libgvc.la \
	$(GVC_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
