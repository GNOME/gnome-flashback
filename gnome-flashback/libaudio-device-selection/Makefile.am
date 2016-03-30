NULL =

noinst_LTLIBRARIES = \
	libaudio-device-selection.la \
	$(NULL)

libaudio_device_selection_la_CPPFLAGS = \
	-I$(top_builddir)/gnome-flashback/libaudio-device-selection \
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
	$(BUILT_SOURCES) \
	$(NULL)

libaudio_device_selection_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libaudio_device_selection_la_LIBADD = \
	$(AUDIO_DEVICE_SELECTION_LIBS) \
	$(NULL)

gf-audio-device-selection-gen.h:
gf-audio-device-selection-gen.c: org.gnome.Shell.AudioDeviceSelection.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) \
		--interface-prefix org.gnome.Shell.AudioDeviceSelection. \
		--generate-c-code gf-audio-device-selection-gen \
		--c-namespace Gf \
		--annotate "org.gnome.Shell.AudioDeviceSelection" \
		           "org.gtk.GDBus.C.Name" AudioDeviceSelectionGen \
		$(srcdir)/org.gnome.Shell.AudioDeviceSelection.xml

BUILT_SOURCES = \
	gf-audio-device-selection-gen.c \
	gf-audio-device-selection-gen.h \
	$(NULL)

EXTRA_DIST = \
	org.gnome.Shell.AudioDeviceSelection.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
