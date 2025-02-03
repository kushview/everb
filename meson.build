project ('everb', ['c', 'cpp'], 
    version : '1.0.2',
    default_options : [
        'cpp_std=c++17', 
        'default_library=static',
        'b_ndebug=if-release',
        'buildtype=release',
        'c_std=c99',
    ])

# Add objective-c if on mac
if host_machine.system() == 'darwin'
  add_languages(['objc', 'objcpp'], native : false, required : true)
  objcc = meson.get_compiler ('objc')
  objcpp = meson.get_compiler ('objcpp')
endif

nodelete_cpp_link_args = meson.get_compiler('cpp').get_supported_link_arguments(['-Wl,-z,nodelete'])

if not meson.is_subproject()
    add_project_arguments (['-DLVTK_STATIC=1', '-DPUGL_STATIC=1'], 
        language : [ 'c', 'cpp' ])
endif

lvtk_opts = [
    'doc=disabled',
    'test=disabled',
]

lvtk_dep = dependency ('lvtk-3', 
    fallback : [ 'lvtk', 'lvtk_dep' ],
    version : '>= 3.0.0',
    default_options : lvtk_opts,
    required : true
)

lui_cairo_dep = dependency ('lui-cairo-0',
    fallback : [ 'lui', 'lui_cairo_dep' ],
    version : '>= 0.0.1',
    default_options : [],
    required : true
)

juce_proj = subproject ('juce')
juce_dep = juce_proj.get_variable ('juce_dep')

clap_dep = dependency ('clap')

subdir ('src')

summary ('Install', clap_install_dir, section: 'CLAP')
summary ('Install', lv2_install_dir, section : 'LV2')
