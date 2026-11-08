project('sleet', 'cpp',
        version:         '0.1.0',
        meson_version:   '>=1.4.0',
        license:         'GPL-3.0-or-later',
        default_options: [ 'cpp_std=c++23', 'warning_level=3' ])

dependencies = [ dependency('kei-utils'), dependency('sdl3'),
                 dependency('sdl3-ttf', fallback: [ 'sdl3_ttf', 'sdl3_ttf_dep' ]) ]

subdir('src')

inc = include_directories('include')
lib = library(meson.project_name(), src,
              include_directories: inc,
              dependencies:        dependencies,
              install:             true)

install_subdir('include/sleet', install_dir: get_option('includedir'))

sleet_dep = declare_dependency(link_with:           lib,
                                include_directories: inc,
                                dependencies:        dependencies)
meson.override_dependency('sleet', sleet_dep)

if get_option('build-tests')
    subdir('test')
endif