project('evisum', 'c',
	version: '0.3.0',
	meson_version : '>= 0.40.0')

efl_version = '>= 1.22.0'

deps = dependency('elementary', version: efl_version)

##### dir locations
dir_prefix = get_option('prefix')
dir_bin    = join_paths(dir_prefix, get_option('bindir'))
dir_lib    = join_paths(dir_prefix, get_option('libdir'))
dir_data   = join_paths(dir_prefix, get_option('datadir'))
dir_locale = join_paths(dir_prefix, get_option('localedir'))

subdir('src')
subdir('data')
