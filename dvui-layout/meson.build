project(
  'dvui-layout',
  'c',
  version: '0.1.0',
  license: 'MIT',
  meson_version: '>=1.3.0',
  # c2x je jen starší jméno C23 (GCC 13); Meson vybere první podporované.
  default_options: ['c_std=c23,c2x', 'warning_level=3', 'buildtype=debugoptimized'],
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

inc = include_directories('include')

# --- jádro layoutu (bez závislostí) -----------------------------------------

core_sources = files(
  'src/core.c',
  'src/data.c',
  'src/layout.c',
  'src/widget_data.c',
  'src/window.c',
  'src/widgets/box.c',
  'src/widgets/button.c',
  'src/widgets/floating.c',
  'src/widgets/grid.c',
  'src/widgets/scroll_area.c',
  'src/widgets/scroll_bar.c',
  'src/widgets/scroll_container.c',
  'src/widgets/scroll_info.c',
  'src/widgets/simple.c',
  'src/backends/testing.c',
)

dvui_layout_lib = static_library(
  'dvui-layout',
  core_sources,
  include_directories: inc,
  dependencies: [m_dep],
)

dvui_layout_dep = declare_dependency(
  link_with: dvui_layout_lib,
  include_directories: inc,
  dependencies: [m_dep],
)

# --- testy (testovací backend, bez GPU) -------------------------------------

test_layout = executable(
  'test_layout',
  'tests/test_layout.c',
  dependencies: [dvui_layout_dep],
)
test('layout', test_layout)

# --- backend SDL 3 GPU a demo -----------------------------------------------

sdl3_dep = dependency('sdl3', required: get_option('sdl3'))

if sdl3_dep.found()
  # Shadery převzaté z DVUI vložíme do C polí malým nástrojem z tools/.
  embed = executable('embed', 'tools/embed.c', native: true)
  shader_sources = []
  foreach stage : ['vertex', 'fragment']
    foreach fmt : ['spv', 'msl', 'dxil']
      shader_sources += custom_target(
        'shader_@0@_@1@'.format(stage, fmt),
        input: 'shaders/default.@0@.@1@'.format(stage, fmt),
        output: 'shader_@0@_@1@.c'.format(stage, fmt),
        command: [embed, '@INPUT@', '@OUTPUT@', 'dvui_shader_@0@_@1@'.format(stage, fmt)],
      )
    endforeach
  endforeach

  dvui_sdl3gpu_lib = static_library(
    'dvui-layout-sdl3gpu',
    'src/backends/sdl3gpu.c',
    shader_sources,
    include_directories: inc,
    dependencies: [sdl3_dep],
  )

  dvui_sdl3gpu_dep = declare_dependency(
    link_with: dvui_sdl3gpu_lib,
    include_directories: inc,
    dependencies: [sdl3_dep, dvui_layout_dep],
  )

  executable(
    'dvui-layout-demo',
    'examples/demo.c',
    dependencies: [dvui_sdl3gpu_dep],
    win_subsystem: 'console',
  )
endif
