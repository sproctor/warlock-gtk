SUBDIRS = util dock src

EXTRA_DIST = \
	$(schema_DATA)

install-data-local:
	@$(NORMAL_INSTALL)
	test -z $(DESTDIR)$(pkgdatadir) || \
	mkdir -p -- $(DESTDIR)$(pkgdatadir)
	if test -d $(srcdir)/images; then \
	for image in $(srcdir)/images/*; do \
	if test -f $$image; then \
	$(INSTALL_DATA) $$image $(DESTDIR)$(pkgdatadir); \
	fi \
	done \
	fi
	if test -d $(srcdir)/scripting; then \
	for script in $(srcdir)/scripting/*; do \
	if test -f $$script; then \
	$(INSTALL_DATA) $$script $(DESTDIR)$(pkgdatadir); \
	fi \
	done \
	fi
	if test -d $(srcdir)/m4; then \
	for file in $(srcdir)/m4/*; do \
	if test -f $$file; then \
	$(INSTALL_DATA) $$file $(DESTDIR)$(pkgdatadir); \
	fi \
	done \
	fi
 
dist-hook:
	if test -d images; then \
	mkdir $(distdir)/images; \
	for image in images/*; do \
	if test -f $$image; then \
	cp -p $$image $(distdir)/images; \
	fi \
	done \
	fi
	if test -d docs; then \
	mkdir $(distdir)/docs; \
	for doc in docs/*; do \
	if test -f $$doc; then \
	cp -p $$doc $(distdir)/docs; \
	fi \
	done \
	fi
	if test -d scripting; then \
	mkdir $(distdir)/scripting; \
	for script in scripting/*; do \
	if test -f $$script; then \
	cp -p $$script $(distdir)/scripting; \
	fi \
	done \
	fi

appsdir = $(datadir)/applications
dist_apps_DATA = warlock.desktop

distpixmapdir = $(datadir)/pixmaps
distpixmap_DATA = images/warlock_icon.png
