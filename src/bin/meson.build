inc = include_directories('.', '../..', 'enigmatic', 'enigmatic/client', 'enigmatic/lz4')

src = []

subdir('engine')
subdir('ui')
subdir('background')
subdir('enigmatic')

evisum_core_src = src

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
   link_with            : [ lz4_lib ],
   install              : true)
