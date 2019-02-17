NULL =

config_files = \
	gnome-flashback.conf \
	gnome-flashback.ini \
	$(NULL)

upgrade_files = \
	$(NULL)

if WITH_COMPIZ_SESSION
configdir = $(COMPIZCONFIG_CONFIG_DIR)
config_DATA = $(config_files)

upgradedir = $(COMPIZCONFIG_UPGRADES_DIR)
upgrade_DATA = $(upgrade_files)
endif

EXTRA_DIST = \
	$(config_files) \
	$(upgrade_files) \
	$(NULL)

-include $(top_srcdir)/git.mk
