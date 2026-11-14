project(
  'dvui-animation',
  'c',
  version: '0.1.0',
  license: 'MIT',
  meson_version: '>=1.3.0',
  # c2x je jen starší jméno C23 (GCC 13); Meson vybere první podporované.
  default_options: ['c_std=c23,c2x', 'warning_level=3', 'buildtype=debugoptimized'],
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# Sdílený kód je v ../shared (viz ../shared/README.md). Meson dovoluje
# v files() a include_directories() cesty mimo adresář projektu.
shared_dir = '../shared'

inc = include_directories('include', shared_dir / 'include')

# --- jádro (bez závislostí) -----------------------------------------

core_sources = files(
  shared_dir / 'src/base.c',
  shared_dir / 'src/geometry.c',
  shared_dir / 'src/data.c',
  shared_dir / 'src/backends/testing.c',
  'src/core.c',
  'src/easing.c',
  'src/animation.c',
  'src/frame.c',
  'src/layout.c',
  'src/widget_data.c',
  'src/window.c',
  'src/widgets/animate.c',
  'src/widgets/box.c',
  'src/widgets/button.c',
  'src/widgets/simple.c',
)

dvui_animation_lib = static_library(
  'dvui-animation',
  core_sources,
  include_directories: inc,
  dependencies: [m_dep],
)

dvui_animation_dep = declare_dependency(
  link_with: dvui_animation_lib,
  include_directories: inc,
  dependencies: [m_dep],
)

# --- testy (testovací backend, bez GPU) -------------------------------------

test_animation = executable(
  'test_animation',
  'tests/test_animation.c',
  dependencies: [dvui_animation_dep],
)
test('animation', test_animation)

# --- backend SDL 3 GPU a demo -----------------------------------------------

sdl3_dep = dependency('sdl3', required: get_option('sdl3'))

if sdl3_dep.found()
  # Shadery převzaté z DVUI vložíme do C polí malým nástrojem z tools/.
  embed = executable('embed', shared_dir / 'tools/embed.c', native: true)
  shader_sources = []
  foreach stage : ['vertex', 'fragment']
    foreach fmt : ['spv', 'msl', 'dxil']
      shader_sources += custom_target(
        'shader_@0@_@1@'.format(stage, fmt),
        input: shared_dir / 'shaders/default.@0@.@1@'.format(stage, fmt),
        output: 'shader_@0@_@1@.c'.format(stage, fmt),
        command: [embed, '@INPUT@', '@OUTPUT@', 'dvui_shader_@0@_@1@'.format(stage, fmt)],
      )
    endforeach
  endforeach

  dvui_sdl3gpu_lib = static_library(
    'dvui-animation-sdl3gpu',
    shared_dir / 'src/backends/sdl3gpu.c',
    shader_sources,
    include_directories: inc,
    dependencies: [sdl3_dep],
  )

  dvui_sdl3gpu_dep = declare_dependency(
    link_with: dvui_sdl3gpu_lib,
    include_directories: inc,
    dependencies: [sdl3_dep, dvui_animation_dep],
  )

  executable(
    'dvui-animation-demo',
    'examples/demo.c',
    dependencies: [dvui_sdl3gpu_dep],
    win_subsystem: 'console',
  )
endif
