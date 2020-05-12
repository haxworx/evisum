inc = include_directories('.', '../..')

src = []

subdir('system')
subdir('ui')

src += files([
   'main.c',
   'evisum_config.c',
   'evisum_config.h',
])

executable('evisum', src,
   include_directories : inc,
   dependencies        : [ deps, deps_os, deps_nls ],
   gui_app             : true,
   install             : true)

if host_os == 'freebsd' or host_os == 'dragonfly'
   meson.add_install_script('perms.sh')
endif
