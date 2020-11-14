project('notekit', 'cpp',
	version : '0.1',
	default_options : ['warning_level=3', 'cpp_std=c++14']
)

latex = option('latex', type: 'combo', choices: ['none', 'clatexmath', 'lasem'], value: 'clatexmath')

deps = [
	dependency('gtkmm-3.0'),
	dependency('gtksourceviewmm-3.0'),
	dependency('jsoncpp'),
	dependency('zlib'),
	dependency('fontconfig')
]

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
