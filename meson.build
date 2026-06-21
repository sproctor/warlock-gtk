project('warlock', 'c',
  version: '0.5',
  license: 'GPL-2.0-or-later',
  meson_version: '>= 0.56.0',
  default_options: ['c_std=gnu11', 'warning_level=1'])

# Dependencies (GTK 3; no GConf, libpcre or libgc any more)
gtk_dep     = dependency('gtk+-3.0')
glib_dep    = dependency('glib-2.0')
gio_dep     = dependency('gio-2.0')
gmodule_dep = dependency('gmodule-2.0')

warlock_deps = [gtk_dep, glib_dep, gio_dep, gmodule_dep]

flex  = find_program('flex')
bison = find_program('bison')

prefix     = get_option('prefix')
datadir    = get_option('datadir')
pkgdatadir = datadir / 'warlock'

# config.h
conf = configuration_data()
conf.set_quoted('PACKAGE', 'warlock')
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE_DATA_DIR', prefix / datadir)
if get_option('buildtype').startswith('debug')
  conf.set('DEBUG', 1)
endif
configure_file(output: 'config.h', configuration: conf)

# include_directories('.') exposes both the source root and the build root
# (where config.h lives)
config_inc = include_directories('.')

add_project_arguments('-DHAVE_CONFIG_H', language: 'c')

subdir('src')

# desktop entry and icon
install_data('warlock.desktop', install_dir: datadir / 'applications')
install_data('images/warlock_icon.png', install_dir: datadir / 'pixmaps')

# UI assets / images live flat in <datadir>/warlock (PACKAGE_DATA_DIR/PACKAGE)
install_subdir('images', install_dir: pkgdatadir, strip_directory: true)
