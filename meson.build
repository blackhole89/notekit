project('notekit', 'cpp',
	version : '0.2.0',
	default_options : ['warning_level=3', 'cpp_std=c++14']
)

conf_data = configuration_data()
conf_data.set('version', meson.project_version())

deps = [
	dependency('gtkmm-3.0'),
	dependency('gtksourceviewmm-3.0'),
	dependency('zlib'),
	dependency('fontconfig')
]

if get_option('clatexmath')
	deps += dependency('clatexmath', version: '>=0.0.3', fallback: ['clatexmath', 'clatexmath_dep'])
	conf_data.set('HAVE_CLATEXMATH', true)
endif

configure_file(input : 'config.h.meson.in',
	output : 'config.h',
	configuration : conf_data
)

subdir('freedesktop')

install_subdir('data/', install_dir: get_option('datadir')/'notekit')
install_subdir('sourceview/', install_dir: get_option('datadir')/'notekit')

install_data('gnome/com.github.blackhole89.notekit.gschema.xml',
  install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas')
)

#if get_option('clatexmath')
#	meson.add_install_script('symlink-clatexmath.sh')
#endif

executable('notekit',
	'about.cpp',
	'drawing.cpp',
	'imagewidgets.cpp',
	'main.cpp',
	'mainwindow.cpp',
	'navigation.cpp',
	'notebook.cpp',
	dependencies: deps,
	install : true
)

meson.add_install_script('build-aux/postinstall.py')
