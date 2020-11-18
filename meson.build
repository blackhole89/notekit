project('notekit', 'cpp',
	version : '0.1',
	default_options : ['warning_level=3', 'cpp_std=c++14']
)

conf_data = configuration_data()
conf_data.set('HAVE_CLATEXMATH', true)

deps = [
	dependency('gtkmm-3.0'),
	dependency('gtksourceviewmm-3.0'),
	dependency('jsoncpp'),
	dependency('zlib'),
	dependency('fontconfig'),
	dependency('clatexmath')
]


configure_file(input : 'config.h.meson.in',
	output : 'config.h',
	configuration : conf_data
)

install_data('freedesktop/notekit.desktop', install_dir : get_option('datadir')/'applications')
install_data('freedesktop/notekit.svg', install_dir : get_option('datadir')/'icons/hicolor/scalable/apps')
install_data('freedesktop/notekit.png', install_dir : get_option('datadir')/'icons/hicolor/128x128/apps')
install_subdir('data/', install_dir: get_option('datadir')/'notekit')
install_subdir('sourceview/', install_dir: get_option('datadir')/'notekit')
meson.add_install_script('symlink-clatexmath.sh')

executable('notekit',
	'drawing.cpp',
	'imagewidgets.cpp',
	'main.cpp',
	'mainwindow.cpp',
	'navigation.cpp',
	'notebook.cpp',
	dependencies: deps,
	install : true
)
