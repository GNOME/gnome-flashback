NULL =

noinst_LTLIBRARIES = \
	liba11y-keyboard.la \
	$(NULL)

liba11y_keyboard_la_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"a11y-keyboard\" \
	-DG_LOG_USE_STRUCTURED=1 \
	-I$(top_srcdir) \
	$(AM_CPPFLAGS) \
	$(NULL)

liba11y_keyboard_la_CFLAGS = \
	$(A11Y_KEYBOARD_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

liba11y_keyboard_la_SOURCES = \
	gf-a11y-keyboard.c \
	gf-a11y-keyboard.h \
	$(NULL)

liba11y_keyboard_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

liba11y_keyboard_la_LIBADD = \
	$(top_builddir)/dbus/libdbus.la \
	$(A11Y_KEYBOARD_LIBS) \
	$(NULL)

-include $(top_srcdir)/git.mk
