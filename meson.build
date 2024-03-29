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
    'demo=disabled',
    'default_library=static'
]

lvtk_dep = dependency ('lvtk-3', 
    fallback : [ 'lvtk', 'lvtk_dep' ],
    version : '>= 3.0.0',
    default_options : lvtk_opts,
    required : true
)

lvtk_cairo_dep = dependency ('lvtk-cairo-3', 
    fallback : [ 'lvtk', 'lvtk_cairo_dep' ],
    version : '>= 3.0.0',
    default_options : lvtk_opts,
    required : true
)

juce_proj = subproject ('juce')
juce_dep = juce_proj.get_variable ('juce_dep')

subdir ('src')

summary ('Install', lv2_install_dir, section : 'LV2')
