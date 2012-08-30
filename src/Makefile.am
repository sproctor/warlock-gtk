INCLUDES = \
        -DPACKAGE_DATA_DIR=\"$(datadir)\" \
        -DPACKAGE_LOCALE_DIR=\"$(prefix)/$(DATADIRNAME)/locale\" \
        @PACKAGE_CFLAGS@ @PCRE_CFLAGS@

BUILT_SOURCES	= script_parser.h script_lexer.h
bin_PROGRAMS    = warlock

dist_pkgdata_DATA = warlock.ui

schemadir   = @GCONF_SCHEMA_FILE_DIR@
schema_DATA = warlock.schemas

EXTRA_DIST = $(schema_DATA)

warlock_SOURCES = \
	compass.c \
	compass.h \
	debug.h \
	entry.c \
	entry.h \
	hand.c \
	hand.h \
	helpers.c \
	helpers.h \
	highlight.c \
	highlight.h \
	log.c \
	log.h \
	macro.c \
	macro.h \
	macro_dialog.c \
	macro_dialog.h \
	main.c \
	preferences.c \
	preferences.h \
	preferences_dialog.c \
	preferences_dialog.h \
	profile_dialog.c \
	profile_dialog.h \
	script.c \
	script.h \
	script_lexer.l \
	script_parser.y \
	sge_connection.c \
	sge_connection.h \
	simu_connection.c \
	simu_connection.h \
	status.c \
	status.h \
	text_strings_dialog.c \
	text_strings_dialog.h \
	warlock.c \
	warlock.h \
	warlockcolorbutton.c \
	warlockcolorbutton.h \
	warlockfontbutton.c \
	warlockfontbutton.h \
	warlockstring.c \
	warlockstring.h \
	warlocktime.c \
	warlocktime.h \
	warlockview.c \
	warlockview.h \
	wizard_protocol_lexer.l

warlock_LDADD = @PACKAGE_LIBS@ @PCRE_LIBS@ @WARLOCK_LIBS_EXTRA@
warlock_LDFLAGS = -rdynamic

AM_YFLAGS	= -d
AM_LFLAGS	= -olex.yy.c
CLEANFILES      = *~
DISTCLEANFILES  = .deps/*.P

script_lexer.h:
	$(LEX) script_lexer.l

install-data-local:
	GCONF_CONFIG_SOURCE=$(GCONF_SCHEMA_CONFIG_SOURCE) \
	$(GCONFTOOL) --makefile-install-rule $(srcdir)/$(schema_DATA) || \
		(echo ;\
		echo "*****************************************************"; \
		echo "Installation of schemas failed, install them manually"; \
		echo "*****************************************************";)
	@true
