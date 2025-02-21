project('commutators', 'cpp')
# Setting the C++ standard should be in default_options,
# but a Meson bug open since 2019 prevents us from putting the setting there
if meson.get_compiler('cpp').get_id() == 'gcc' or meson.get_compiler('cpp').get_id() == 'clang'
  add_project_arguments('-std=c++20', language : 'cpp')
  add_project_arguments('-march=native', language : 'cpp')
endif

dependencies = []

dependencies += dependency('gmp')
# dependencies += subproject('catch2').get_variable('catch2_with_main_dep')

src_files = [
    'main.cpp',
    'SerializationHelpers.cpp',
    'BigInt.cpp',
    'Expression.cpp',
    'Poly.cpp',
]
executable('main', src_files, dependencies : dependencies)

#executable('collect_stats', 'collect_stats.cpp')
