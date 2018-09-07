NULL =

noinst_LTLIBRARIES = \
	liba11y-keyboard.la \
	$(NULL)

liba11y_keyboard_la_CPPFLAGS = \
	$(NULL)

liba11y_keyboard_la_CFLAGS = \
	$(A11Y_KEYBOARD_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

liba11y_keyboard_la_SOURCES = \
	gf-a11y-keyboard.c \
	gf-a11y-keyboard.h \
	$(BUILT_SOURCES) \
	$(NULL)

liba11y_keyboard_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

liba11y_keyboard_la_LIBADD = \
	$(A11Y_KEYBOARD_LIBS) \
	$(NULL)

gf-notifications-gen.h:
gf-notifications-gen.c: org.freedesktop.Notifications.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-notifications-gen \
		$(srcdir)/org.freedesktop.Notifications.xml

BUILT_SOURCES = \
	gf-notifications-gen.c \
	gf-notifications-gen.h \
	$(NULL)

EXTRA_DIST = \
	org.freedesktop.Notifications.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
