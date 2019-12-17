NULL =

layout_files = \
	gnome-flashback.layout \
	$(NULL)

if WITH_SYSTEM_INDICATORS
layoutsdir = $(GNOME_PANEL_LAYOUTS_DIR)
layouts_DATA = $(layout_files)
endif

EXTRA_DIST = \
	$(layout_files) \
	$(NULL)

-include $(top_srcdir)/git.mk
