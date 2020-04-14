inc = include_directories('.', '../..')

executable('evisum', [
     'configuration.c',
     'configuration.h',
     'disks.c',
     'disks.h',
     'process.c',
     'process.h',
     'system.c',
     'system.h',
     'ui.c',
     'ui.h',
     'main.c'
   ],
   include_directories : inc,
   dependencies        : [ deps, deps_os ],
   gui_app             : true,
   install             : true)
