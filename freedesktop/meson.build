app_id = 'com.github.blackhole89.notekit'
appstream_file = files('@0@.metainfo.xml'.format(app_id))
desktop_file = files('@0@.desktop'.format(app_id))

install_data(appstream_file, install_dir : get_option('datadir')/'metainfo')
install_data(desktop_file, install_dir : get_option('datadir')/'applications')
install_data('@0@.svg'.format(app_id), install_dir : get_option('datadir')/'icons/hicolor/scalable/apps')
install_data('@0@.png'.format(app_id), install_dir : get_option('datadir')/'icons/hicolor/128x128/apps')


desktop_utils = find_program('desktop-file-validate', required: false)
if desktop_utils.found()
  test('Validate desktop file', desktop_utils,
    args: [desktop_file]
  )
endif

appstream_util = find_program('appstream-util', required: false)
if appstream_util.found()
  test('Validate appstream file', appstream_util,
    args: ['validate-relax', appstream_file]
  )
endif
