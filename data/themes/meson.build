edje_cmd = find_program('edje_cc')

cmd = [ edje_cmd,
       '-id', join_paths(meson.source_root(), 'data', 'images'),
       '-id', join_paths(meson.source_root(), 'data', 'icons'),
       '@INPUT@', '@OUTPUT@'
      ]

custom_target('evisum.edj',
              input       : 'evisum.edc',
              output      : 'evisum.edj',
              command     : cmd,
              install_dir : join_paths(dir_data, 'evisum', 'themes'),
              install     : true
             )
