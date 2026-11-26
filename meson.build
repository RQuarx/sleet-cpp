project('sleet', 'cpp',
        version:         '1.3.1',
        meson_version:   '>=1.4.0',
        license:         'GPL-3.0-or-later',
        default_options: [ 'cpp_std=c++23', 'warning_level=3' ])

# Third-party headers are included as system headers, so their warnings stay
# out of Sleet's warning_level=3 builds.
dependencies = [
  dependency('kei-utils', fallback: [ 'kei-utils', 'kei_utils' ], include_type: 'system'),
  dependency('sdl3', fallback: [ 'sdl3', 'sdl3_dep' ], include_type: 'system'),
  dependency('sdl3-ttf', fallback: [ 'sdl3_ttf', 'sdl3_ttf_dep' ], include_type: 'system'),
  dependency('libunibreak', fallback: [ 'libunibreak', 'unibreak_dep' ], include_type: 'system'),
  dependency('utf8cpp', fallback: [ 'utfcpp', 'utfcpp_dep' ], include_type: 'system'),
]

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

# kei-utils and utf8cpp are header-only and ship no pkg-config files. Meson can
# only list the compiled dependencies in Requires.private, so consumers must
# provide those headers on their include path.
pkg = import('pkgconfig')
pkg.generate(lib,
             name:        meson.project_name(),
             description: 'A C++23 UI framework utilizing the Elm architecture',
             subdirs:     '.')

if get_option('build-example')
  executable('sleet-example', 'example.cc', dependencies: sleet_dep)
endif

if get_option('build-tests')
  subdir('test')
endif
