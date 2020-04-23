inc = include_directories('.', '../..')

executable('evisum', [
     'main.c',
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
     'gettext.h',
   ],
   include_directories : inc,
   dependencies        : [ deps, deps_os, deps_nls ],
   gui_app             : true,
   install             : true)

if host_os == 'freebsd' or host_os == 'dragonfly'
   meson.add_install_script('perms.sh')
endif
