project('exert', 'cpp')
deps = [dependency('x11'), dependency('lua'), dependency('xcb'), dependency('xcb-keysyms'), dependency('xcb-icccm'), dependency('xcb-randr')]

executable(
  'exert',
  ['main.cpp', 'config.cpp', 'utils.cpp'],
  dependencies: deps,
  include_directories: '.',
)
