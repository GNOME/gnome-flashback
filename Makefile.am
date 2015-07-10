NULL =

SUBDIRS = \
	data \
	gnome-flashback \
	po \
	$(NULL)

ACLOCAL_AMFLAGS = --install -I m4 ${ACLOCAL_FLAGS}

MAINTAINERCLEANFILES = \
	$(GITIGNORE_MAINTAINERCLEANFILES_TOPLEVEL) \
	$(GITIGNORE_MAINTAINERCLEANFILES_MAKEFILE_IN) \
	$(GITIGNORE_MAINTAINERCLEANFILES_M4_LIBTOOL) \
	`find "m4" -type f -name "*.m4" -print` \
	$(srcdir)/INSTALL \
	$(srcdir)/config.h.in~ \
	$(srcdir)/configure \
	$(NULL)

EXTRA_DIST = \
	$(wildcard $(top_srcdir)/modulesets/*.modules) \
	$(NULL)

GITIGNOREFILES = $(PACKAGE)-\*.tar.{gz,bz2,xz}

-include $(top_srcdir)/git.mk
