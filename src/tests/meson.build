exel_test = executable('widget_exel_test',
   'widget_exel_test.c',
   '../bin/ui/evisum_ui_widget_exel.c',
   '../bin/ui/evisum_ui_cache.c',
   include_directories : inc,
   dependencies : deps,
   install : false)

test('exel smart object', exel_test,
   args : ['--test'],
   env : ['EINA_LOG_LEVEL=0', 'ASAN_OPTIONS=detect_leaks=0'],
   timeout : 60)

test('exel demo smoke', exel_test,
   args : ['--demo-smoke'],
   env : ['EINA_LOG_LEVEL=0', 'ASAN_OPTIONS=detect_leaks=0'],
   timeout : 60)
