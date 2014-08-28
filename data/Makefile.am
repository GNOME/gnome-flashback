sessiondir       = $(datadir)/gnome-session/sessions
session_in_files = gnome-flashback-compiz.session.in gnome-flashback-metacity.session.in
session_DATA     = $(session_in_files:.session.in=.session)

%.session: %.session.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

xsessiondir       = $(datadir)/xsessions
xsession_in_files = gnome-flashback-compiz.desktop.in gnome-flashback-metacity.desktop.in
xsession_DATA     = $(xsession_in_files:.desktop.in=.desktop)

%.desktop: %.desktop.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*.po) ; LC_ALL=C $(INTLTOOL_MERGE) -d -u -c $(top_builddir)/po/.intltool-merge-cache $(top_srcdir)/po $< $@

@INTLTOOL_XML_NOMERGE_RULE@

gsettings_schemas_in_in = org.gnome.gnome-flashback.gschema.xml.in.in
gsettings_schemas_in    = $(gsettings_schemas_in_in:.xml.in.in=.xml.in)
gsettings_SCHEMAS       = $(gsettings_schemas_in:.xml.in=.xml)

%.gschema.xml.in: %.gschema.xml.in.in Makefile
	$(AM_V_GEN) $(SED) -e 's^\@GETTEXT_PACKAGE\@^$(GETTEXT_PACKAGE)^g' < $< > $@

@GSETTINGS_RULES@

EXTRA_DIST = \
	$(session_in_files) \
	$(xsession_in_files) \
	$(gsettings_schemas_in_in)

CLEANFILES = \
	$(session_DATA) \
	$(xsession_DATA) \
	$(gsettings_schemas_in) \
	$(gsettings_SCHEMAS) \
	*.gschema.valid

-include $(top_srcdir)/git.mk
