##### Project
project('evisum', 'c',
	version      : '0.4.0',
	meson_version : '>= 0.40.0')

efl_version = '>= 1.22.0'

cfg = configuration_data()

host_os = host_machine.system()

cc = meson.get_compiler('c')

deps = dependency('elementary', version: efl_version)

deps_os = declare_dependency(link_args : [] )
if host_os == 'openbsd' or host_os == 'freebsd' or host_os == 'dragonfly'
  deps_os = declare_dependency(link_args : [ '-lkvm' ])
endif

##### dir locations
dir_prefix = get_option('prefix')
dir_bin    = join_paths(dir_prefix, get_option('bindir'))
dir_lib    = join_paths(dir_prefix, get_option('libdir'))
dir_data   = join_paths(dir_prefix, get_option('datadir'))
dir_locale = join_paths(dir_prefix, get_option('localedir'))

##### translation

deps_nls = []
translate_enabled = false
intl_lib = cc.find_library('intl', required: false)
if intl_lib.found()
  translate_enabled = true
  deps_nls = [intl_lib]
else
  gettext_code = '''
  #include <libintl.h>
  int main(int argc, char *argv[]) {
      (void)ngettext("", "", 0);
      return 0;
  }
  '''
  if cc.links(gettext_code)
    translate_enabled = true
    message('Intl Support Enabled')
  endif
endif

##### config.h

cfg.set_quoted('PACKAGE', meson.project_name())
cfg.set_quoted('PACKAGE_VERSION', meson.project_version())
cfg.set_quoted('PACKAGE_DATA_DIR', join_paths(dir_data, 'evisum'))

if translate_enabled
  subdir('po')
endif

subdir('src')
subdir('data')

configure_file(output: 'config.h', configuration: cfg)
