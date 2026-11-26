test_dependencies = [ dependency('sleet'), dependency('boost.ut', include_type: 'system') ]

tests = {
  'application':   'application.cc',
  'animation':     'animation.cc',
  'line-breaking': 'line-breaking.cc',
  'ustring':       'ustring.cc',
  'color':         'color.cc',
  'control':       'control.cc',
  'nodes':         'nodes.cc',
  'layout':        'layout.cc',
  'wrap':          'wrap.cc',
  'render':        'render.cc',
  'scroll':        'scroll.cc',
  'theme':         'theme.cc',
  'widgets':       'widgets.cc',
}

foreach name, source : tests
  test_exe = executable(name, source, dependencies: test_dependencies)
  test(name, test_exe,
       env: [ 'SDL_VIDEODRIVER=dummy', 'SDL_RENDER_DRIVER=software' ])
endforeach
