bin_PROGRAMS = \
	gnome-flashback-init

AM_CPPFLAGS = \
	$(GNOME_FLASHBACK_INIT_CFLAGS) \
	-I$(top_builddir)/gnome-flashback-init

gnome_flashback_init_SOURCES = \
	main.c

gnome_flashback_init_LDADD = \
	$(GNOME_FLASHBACK_INIT_LIBS)

-include $(top_srcdir)/git.mk
