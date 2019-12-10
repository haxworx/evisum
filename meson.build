##### Project
project('evisum', 'c',
	versioni      : '0.3.0',
	meson_version : '>= 0.40.0')

efl_version = '>= 1.22.0'

cfg = configuration_data()

host_os = host_machine.system()

deps = dependency('elementary', version: efl_version)

deps_os = declare_dependency(link_args : [] )
if host_os == 'openbsd'
  deps_os = declare_dependency(link_args : [ '-lkvm' ])
endif

##### dir locations
dir_prefix = get_option('prefix')
dir_bin    = join_paths(dir_prefix, get_option('bindir'))
dir_lib    = join_paths(dir_prefix, get_option('libdir'))
dir_data   = join_paths(dir_prefix, get_option('datadir'))
dir_locale = join_paths(dir_prefix, get_option('localedir'))

##### config.h

cfg.set_quoted('PACKAGE_VERSION', meson.project_version())
configure_file(output: 'config.h', configuration: cfg)

subdir('src')
subdir('data')
