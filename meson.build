project('notekit', 'cpp',
	version : '0.2.0',
	default_options : ['warning_level=3', 'cpp_std=c++14']
)

has_devenv = meson.version().version_compare('>=0.58.0')
if has_devenv
	devenv = environment()
	devenv.set('NK_DEVEL', meson.project_source_root())
	devenv.set('NK_SETTINGS', 'json')
endif

conf_data = configuration_data()
conf_data.set('version', meson.project_version())

gnome = import('gnome')

deps = [
	dependency('gtkmm-3.0'),
	dependency('gtksourceviewmm-3.0'),
	dependency('zlib'),
	dependency('fontconfig'),
	dependency('gdk-x11-3.0', required : false),
	dependency('jsoncpp')
]

if get_option('clatexmath')
	deps += dependency('clatexmath', version: '>=0.0.3', fallback: ['clatexmath', 'clatexmath_dep'])
	conf_data.set('HAVE_CLATEXMATH', true)
endif

conf_data.set('settings_provider', get_option('settings_provider'))
conf_data.set('MIGRATE_LEGACY_SETTINGS', get_option('migrate_legacy_settings'))

configure_file(input : 'config.h.meson.in',
	output : 'config.h',
	configuration : conf_data
)

subdir('freedesktop')

install_subdir('data/', install_dir: get_option('datadir')/'notekit')
install_subdir('sourceview/', install_dir: get_option('datadir')/'notekit')

subdir('gnome')

executable('notekit',
	'about.cpp',
	'drawing.cpp',
	'imagewidgets.cpp',
	'main.cpp',
	'mainwindow.cpp',
	'navigation.cpp',
	'notebook.cpp',
	'settings.cpp',
	dependencies: deps,
	install : true
)

if has_devenv
	meson.add_devenv(devenv)
endif
