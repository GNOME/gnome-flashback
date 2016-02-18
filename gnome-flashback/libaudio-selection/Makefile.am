NULL =

noinst_LTLIBRARIES = \
	libaudio-selection.la \
	$(NULL)

libaudio_selection_la_CPPFLAGS = \
	-I$(top_builddir)/gnome-flashback/libaudio-selection \
	$(NULL)

libaudio_selection_la_CFLAGS = \
	$(AUDIO_SELECTION_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libaudio_selection_la_SOURCES = \
	gf-audio-selection.c \
	gf-audio-selection.h \
	$(NULL)

libaudio_selection_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libaudio_selection_la_LIBADD = \
	$(AUDIO_SELECTION_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
