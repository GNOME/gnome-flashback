noinst_LTLIBRARIES = \
	libautomount-manager.la

AM_CPPFLAGS = \
	$(AUTOMOUNT_MANAGER_CFLAGS) \
	-I$(top_builddir)/gnome-flashback/libautomount-manager

libautomount_manager_la_SOURCES = \
	gsd-automount-manager.c \
	gsd-automount-manager.h \
	gsd-autorun.c \
	gsd-autorun.h

libautomount_manager_la_LIBADD =	\
	$(AUTOMOUNT_MANAGER_LIBS)

-include $(top_srcdir)/git.mk
