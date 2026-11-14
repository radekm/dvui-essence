project(
  'dvui-layout',
  'c',
  version: '0.1.0',
  license: 'MIT',
  meson_version: '>=1.4.0',
  # C23 s #embed: GCC >= 15, Clang >= 19
  default_options: ['c_std=c23', 'warning_level=3', 'buildtype=debugoptimized'],
)

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required: false)

# Sdílený kód je v ../shared (viz ../shared/README.md). Meson dovoluje
# v files() a include_directories() cesty mimo adresář projektu.
shared_dir = '../shared'

inc = include_directories('include', shared_dir / 'include')

# --- jádro layoutu (bez závislostí) -----------------------------------------

core_sources = files(
  shared_dir / 'src/base.c',
  shared_dir / 'src/geometry.c',
  shared_dir / 'src/data.c',
  shared_dir / 'src/backends/testing.c',
  'src/core.c',
  'src/frame.c',
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
  # Shadery z ../shared/shaders vkládá sdl3gpu.c přes C23 #embed.
  dvui_sdl3gpu_lib = static_library(
    'dvui-layout-sdl3gpu',
    shared_dir / 'src/backends/sdl3gpu.c',
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
