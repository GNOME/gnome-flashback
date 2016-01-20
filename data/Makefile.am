NULL =

SUBDIRS = \
	applications \
	autostart \
	directories \
	menus \
	schemas \
	sessions \
	theme \
	xsessions \
	$(NULL)

libexec_SCRIPTS = \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	$(NULL)

gnome-flashback-compiz: Makefile
	$(AM_V_GEN) (echo '#!/bin/sh' && echo '' && \
	             echo 'if [ -z $$XDG_CURRENT_DESKTOP ]; then' && \
	             echo '  export XDG_CURRENT_DESKTOP="GNOME-Flashback:GNOME"' && \
	             echo 'fi' && echo '' && \
	             echo 'exec gnome-session --session=gnome-flashback-compiz "$$@"') > $@
	$(AM_V_at) chmod a+x $@

gnome-flashback-metacity: Makefile
	$(AM_V_GEN) (echo '#!/bin/sh' && echo '' && \
	             echo 'if [ -z $$XDG_CURRENT_DESKTOP ]; then' && \
	             echo '  export XDG_CURRENT_DESKTOP="GNOME-Flashback:GNOME"' && \
	             echo 'fi' && echo '' && \
	             echo 'exec gnome-session --session=gnome-flashback-metacity --disable-acceleration-check "$$@"') > $@
	$(AM_V_at) chmod a+x $@

CLEANFILES = \
	gnome-flashback-compiz \
	gnome-flashback-metacity \
	$(NULL)

-include $(top_srcdir)/git.mk
