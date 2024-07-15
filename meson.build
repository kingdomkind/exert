project('exert', 'cpp')
deps = [dependency('x11'), dependency('lua'), dependency('xcb)]

executable(
  'exert',
  ['main.cpp', 'config.cpp', 'utils.cpp', 'keybinds.cpp'],
  dependencies: deps,
  include_directories: '.',
)
