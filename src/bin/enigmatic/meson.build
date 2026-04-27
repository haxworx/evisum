dep_eina = dependency('eina', version: efl_version)
dep_eet = dependency('eet', version: efl_version)
dep_ecore = dependency('ecore', version: efl_version)
dep_ecore_file = dependency('ecore-file', version: efl_version)
dep_ecore_con = dependency('ecore-con', version: efl_version)
dep_efreet = dependency('efreet', version: efl_version)
dep_eio = dependency('eio', version: efl_version)
dep_elm = dependency('elementary', version: efl_version)

subdir('lz4')

enigmatic_config_dir = include_directories('.')
enigmatic_inc_lz4 = include_directories('lz4')
enigmatic_inc_monitor = include_directories('monitor')

enigmatic_src = []

subdir('system')
subdir('monitor')

enigmatic_src += src_system
enigmatic_src += src_monitor
enigmatic_src += src_process

src_log = files([
   'enigmatic_log.c',
   'enigmatic_log.h',
])

enigmatic_src += src_log

src_generic = files([
   'enigmatic_util.c',
])

enigmatic_src += src_generic

enigmatic_src += files([
   'Enigmatic.h',
   'enigmatic_config.c',
   'enigmatic_config.h',
   'enigmatic_server.c',
   'enigmatic_server.h',
   'enigmatic_query.c',
   'enigmatic_query.h',
   'enigmatic_main.c',
   'uid.c',
])

executable('enigmatic', enigmatic_src,
   include_directories  : [ enigmatic_config_dir, enigmatic_inc_monitor, enigmatic_inc_lz4 ],
   dependencies         : [ dep_eina, dep_eet, dep_ecore, dep_ecore_file, dep_ecore_con, deps_os ],
   link_with            : lz4_lib,
   gui_app              : false,
   install              : true)

enigmatic_src_start = files([
   'enigmatic_start.c',
])

enigmatic_src_start += src_generic
enigmatic_src_start += src_system

executable('enigmatic_start', enigmatic_src_start,
   include_directories  : [ enigmatic_config_dir ],
   dependencies         : [ dep_eina, dep_ecore, dep_ecore_file ],
   gui_app              : false,
   install              : true)

if host_os == 'freebsd' or host_os == 'dragonfly'
   meson.add_install_script('perms.sh')
endif

subdir('client')
subdir('examples')
subdir('tests')
