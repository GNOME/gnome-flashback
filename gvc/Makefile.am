NULL =

noinst_LTLIBRARIES = \
	libgvc.la \
	$(NULL)

libgvc_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gvc\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_builddir)/gvc \
	-I$(top_srcdir)/gvc/gvc \
	$(NULL)

libgvc_la_CFLAGS = \
	$(WARN_CFLAGS) \
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
	$(BUILT_SOURCES) \
	$(NULL)

libgvc_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libgvc_la_LIBADD = \
	$(GVC_LIBS) \
	$(NULL)

if HAVE_ALSA
libgvc_la_CFLAGS += \
	$(ALSA_CFLAGS) \
	$(NULL)

libgvc_la_LIBADD += \
	$(ALSA_LIBS) \
	$(NULL)

noinst_PROGRAMS = \
	test-audio-device-selection \
	$(NULL)

test_audio_device_selection_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"gvc\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir)/gvc/gvc \
	$(NULL)

test_audio_device_selection_CFLAGS = \
	$(ALSA_CFLAGS) \
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
	$(ALSA_LIBS) \
	$(GVC_LIBS) \
	$(NULL)
endif

ENUM_TYPES = \
	$(srcdir)/gvc/gvc-mixer-stream.h \
	$(NULL)

gvc-enum-types.c: gvc-enum-types.c.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gvc-enum-types.c.in $(ENUM_TYPES) > \
		gvc-enum-types.c.tmp && mv gvc-enum-types.c.tmp gvc-enum-types.c

gvc-enum-types.h: gvc-enum-types.h.in $(ENUM_TYPES) Makefile.am
	$(AM_V_GEN) $(GLIB_MKENUMS) --template $(srcdir)/gvc-enum-types.h.in $(ENUM_TYPES) > \
		gvc-enum-types.h.tmp && mv gvc-enum-types.h.tmp gvc-enum-types.h

BUILT_SOURCES = \
	gvc-enum-types.c \
	gvc-enum-types.h \
	$(NULL)

EXTRA_DIST = \
	gvc-enum-types.c.in \
	gvc-enum-types.h.in \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
