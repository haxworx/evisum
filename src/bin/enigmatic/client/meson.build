inc_client = [ include_directories('.', '../'), enigmatic_config_dir, enigmatic_inc_lz4 ]
dep_client = [ dep_eina, dep_ecore, dep_ecore_file, dep_efreet, dep_eio ]

src_client = []

src_client += src_log
src_client += src_generic
src_client += src_system

src_client += files([
   'enigmatic_client.c',
])

enigmatic_client_lib = shared_library(
   'enigmatic_client',
   src_client,
   include_directories : inc_client,
   dependencies        : dep_client,
   link_with           : lz4_lib,
   install             : true,
   version             : meson.project_version(),
   soversion           : '0',
)

enigmatic_client_dep = declare_dependency(
   link_with           : enigmatic_client_lib,
   include_directories : [ include_directories('.'), include_directories('..') ],
   dependencies        : dep_client,
)

install_headers('../Events.h', subdir : 'enigmatic')
install_headers('../enigmatic_util.h', subdir : 'enigmatic')
install_headers('Enigmatic_Client.h', subdir : 'enigmatic')
install_headers('../system/machine.h', '../system/file_systems.h', '../system/process.h', subdir : 'enigmatic/system')
install_headers('../intl/gettext.h', subdir : 'enigmatic/intl')

pkg = import('pkgconfig')
pkg.generate(
   enigmatic_client_lib,
   name        : 'enigmatic_client',
   description : 'Enigmatic streaming client API',
   subdirs     : 'enigmatic',
   version     : meson.project_version(),
)
