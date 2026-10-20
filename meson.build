project('commutators', 'cpp', version : '1.1',
  default_options : ['warning_level=1'])

# Setting the C++ standard should be in default_options,
# but a Meson bug open since 2019 prevents us from putting the setting there
if meson.get_compiler('cpp').get_id() == 'gcc' or meson.get_compiler('cpp').get_id() == 'clang'
  add_project_arguments('-std=c++20', language : 'cpp')
  add_project_arguments('-march=native', language : 'cpp')
endif

# Pass -DDEBUG to enable verbose [DIAG] diagnostic output.
# Usage: meson setup --buildtype=debug build-debug -Ddebug_output=true
option_debug = get_option('debug_output')
if option_debug
  add_project_arguments('-DDEBUG', language : 'cpp')
  message('Debug diagnostic output enabled (-DDEBUG)')
endif

dependencies = []
dependencies += dependency('gmp')

src_files = [
    'main.cpp',
    'SerializationHelpers.cpp',
    'BigInt.cpp',
    'Expression.cpp',
    'Poly.cpp',
    'Poly2.cpp',
]
executable('main', src_files, dependencies : dependencies)

