install_data('com.github.blackhole89.notekit.gschema.xml', install_dir: join_paths(get_option('datadir'), 'glib-2.0/schemas'))
gnome.compile_schemas(build_by_default: true, depend_files: 'com.github.blackhole89.notekit.gschema.xml')

desktop_database = find_program('update-desktop-database', required: false)
# if update-desktop-database is installed, the other tools are probably installed to
# fixme once someone actually hit this otherwise.
if meson.version().version_compare('>=0.57.0') and desktop_database.found()
	if meson.version().version_compare('>=0.59.0')
		gnome.post_install(
			glib_compile_schemas: true,
			gtk_update_icon_cache: true,
			update_desktop_database: true
		)
	else
		gnome.post_install(
			glib_compile_schemas: true,
			gtk_update_icon_cache: true
		)
	endif
endif
