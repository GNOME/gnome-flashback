NULL =

noinst_LTLIBRARIES = \
	libdbus.la \
	$(NULL)

libdbus_la_CPPFLAGS = \
	$(AM_CPPCFLAGS) \
	$(NULL)

libdbus_la_CFLAGS = \
	$(DBUS_CFLAGS) \
	$(WARN_CFLAGS) \
	$(AM_CFLAGS) \
	$(NULL)

libdbus_la_SOURCES = \
	$(BUILT_SOURCES) \
	$(NULL)

libdbus_la_LDFLAGS = \
	$(WARN_LDFLAGS) \
	$(AM_LDFLAGS) \
	$(NULL)

libdbus_la_LIBADD = \
	$(DBUS_LIBS) \
	$(NULL)

gf-file-manager-gen.h:
gf-file-manager-gen.c: org.freedesktop.FileManager1.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-file-manager-gen \
		$(srcdir)/org.freedesktop.FileManager1.xml

gf-nautilus-gen.h:
gf-nautilus-gen.c: org.gnome.Nautilus.FileOperations.xml
	$(AM_V_GEN) $(GDBUS_CODEGEN) --c-namespace Gf \
		--generate-c-code gf-nautilus-gen \
		$(srcdir)/org.gnome.Nautilus.FileOperations.xml

BUILT_SOURCES = \
	gf-file-manager-gen.c \
	gf-file-manager-gen.h \
	gf-nautilus-gen.c \
	gf-nautilus-gen.h \
	$(NULL)

EXTRA_DIST = \
	org.freedesktop.FileManager1.xml \
	org.gnome.Nautilus.FileOperations.xml \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	$(NULL)

-include $(top_srcdir)/git.mk
