NULL =

SUBDIRS = \
	applications \
	autostart \
	compizconfig \
	directories \
	menus \
	schemas \
	sessions \
	theme \
	ui \
	xsessions \
	$(NULL)

libexec_SCRIPTS = \
	gnome-flashback-metacity \
	$(NULL)

gnome-flashback-metacity: Makefile
	$(AM_V_GEN) (echo '#!/bin/sh' && echo '' && \
	             echo 'if [ -z $$XDG_CURRENT_DESKTOP ]; then' && \
	             echo '  export XDG_CURRENT_DESKTOP="GNOME-Flashback:GNOME"' && \
	             echo 'fi' && echo '' && \
	             echo 'exec gnome-session --session=gnome-flashback-metacity --disable-acceleration-check "$$@"') > $@
	$(AM_V_at) chmod a+x $@

CLEANFILES = \
	gnome-flashback-metacity \
	$(NULL)

if WITH_COMPIZ_SESSION
libexec_SCRIPTS += \
	gnome-flashback-compiz \
	$(NULL)

gnome-flashback-compiz: Makefile
	$(AM_V_GEN) (echo '#!/bin/sh' && echo '' && \
	             echo 'if [ -z $$XDG_CURRENT_DESKTOP ]; then' && \
	             echo '  export XDG_CURRENT_DESKTOP="GNOME-Flashback:GNOME"' && \
	             echo 'fi' && echo '' && \
	             echo 'exec gnome-session --session=gnome-flashback-compiz "$$@"') > $@
	$(AM_V_at) chmod a+x $@

CLEANFILES += \
	gnome-flashback-compiz \
	$(NULL)
endif

-include $(top_srcdir)/git.mk
