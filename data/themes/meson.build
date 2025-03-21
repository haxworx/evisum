edje_cmd = find_program('edje_cc')

cmd = [ edje_cmd,
       '-id', join_paths(meson.project_source_root(), 'data', 'images'),
       '-id', join_paths(meson.project_source_root(), 'data', 'icons'),
       '-sd', join_paths(meson.project_source_root(), 'data', 'sounds'),
       '@INPUT@', '@OUTPUT@'
      ]

custom_target('evisum.edj',
              input       : 'evisum.edc',
              output      : 'evisum.edj',
              command     : cmd,
              install_dir : join_paths(dir_data, 'evisum', 'themes'),
              install     : true
             )
