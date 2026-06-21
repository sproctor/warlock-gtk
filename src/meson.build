# Generated scanners / parser.  script_parser.y and script_lexer.l reference
# each other's headers, so both headers must be produced before either .c is
# compiled; listing the custom_target outputs as sources handles the ordering.

script_parser = custom_target('script_parser',
  input: 'script_parser.y',
  output: ['script_parser.c', 'script_parser.h'],
  command: [bison, '--defines=@OUTPUT1@', '-o', '@OUTPUT0@', '@INPUT@'])

script_lexer = custom_target('script_lexer',
  input: 'script_lexer.l',
  output: ['script_lexer.c', 'script_lexer.h'],
  command: [flex, '--header-file=@OUTPUT1@', '-o', '@OUTPUT0@', '@INPUT@'])

wizard_lexer = custom_target('wizard_protocol_lexer',
  input: 'wizard_protocol_lexer.l',
  output: 'wizard_protocol_lexer.c',
  command: [flex, '-o', '@OUTPUT@', '@INPUT@'])

warlock_sources = files(
  'compass.c',
  'entry.c',
  'hand.c',
  'helpers.c',
  'highlight.c',
  'log.c',
  'macro.c',
  'macro_dialog.c',
  'main.c',
  'preferences.c',
  'preferences_dialog.c',
  'profile_dialog.c',
  'script.c',
  'sge_connection.c',
  'simu_connection.c',
  'status.c',
  'text_strings_dialog.c',
  'warlock.c',
  'warlockcolorbutton.c',
  'warlockfontbutton.c',
  'warlockstring.c',
  'warlocktime.c',
  'warlockview.c',
)

executable('warlock',
  warlock_sources,
  script_parser,
  script_lexer,
  wizard_lexer,
  dependencies: warlock_deps,
  include_directories: [config_inc, include_directories('.')],
  export_dynamic: true,
  install: true)

# warlock.ui is loaded at runtime from PACKAGE_DATA_DIR/PACKAGE
install_data('warlock.ui', install_dir: pkgdatadir)
