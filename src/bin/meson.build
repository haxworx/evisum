inc = include_directories('.', '../..')

src = []

subdir('system')
subdir('ui')
subdir('background')

executable('evisum_test', src + files([
   'evisum_ui_widget_example.c',
   'evisum_config.c',
   'evisum_server.c',
]),
   include_directories  : inc,
   dependencies         : [ deps, deps_os, deps_nls ],
   install              : false)

src += files([
   'main.c',
   'evisum_config.c',
   'evisum_config.h',
   'evisum_server.c',
   'evisum_server.h',
])

executable('evisum', src,
   include_directories  : inc,
   dependencies         : [ deps, deps_os, deps_nls ],
   install              : true)

if host_os == 'freebsd' or host_os == 'dragonfly'
   meson.add_install_script('perms.sh')
endif
