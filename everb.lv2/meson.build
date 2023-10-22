fs = import ('fs')

everb_sources = files ('''
    plugin.cpp
    everb.cpp
'''.split())

everb_ui_type = 'X11UI'
if host_machine.system() == 'windows'
    everb_ui_type = 'WindowsUI'
elif host_machine.system() == 'darwin'
    everb_ui_type = 'CocoaUI'
endif

plugin_install_dir = fs.expanduser (get_option ('lv2dir'))
if plugin_install_dir == ''
    if target_machine.system() == 'darwin'
        plugin_install_dir = '/Library/Audio/Plug-Ins/LV2'
    elif target_machine.system() == 'haiku'
        plugin_install_dir = '/boot/common/add-ons/lv2'
    elif target_machine.system() == 'windows'
        plugin_install_dir = 'C:/Program Files/Common Files/LV2'
    else
        plugin_install_dir = get_option('prefix') / 'lib' / 'lv2'
    endif
endif

plugin_install_dir = plugin_install_dir / 'everb.lv2'

plugin = shared_module ('everb',
    everb_sources,
    name_prefix : '',
    dependencies : [ lvtk_dep, juce_dep ],
    install : true,
    install_dir : plugin_install_dir,
    gnu_symbol_visibility : 'hidden'
)

ui = shared_module ('everb-ui',
    'ui.cpp',
    name_prefix : '',
    dependencies : [ lvtk_opengl_dep ],
    include_directories : [ '.' ],
    install : true,
    install_dir : plugin_install_dir,
    cpp_args : [ ],
    gnu_symbol_visibility : 'hidden'
)

manifest_ttl = configure_file (
    input : 'manifest.ttl.in',
    output : 'manifest.ttl',
    configuration : {
        'BINARY' : fs.name (plugin.full_path()),
        'UI_BINARY' : fs.name (ui.full_path()),
        'UI_TYPE' : everb_ui_type
    },
    install : true,
    install_dir : plugin_install_dir
)

everb_ttl = configure_file (
    input : 'everb.ttl.in',
    output : 'everb.ttl',
    copy : true,
    install : true,
    install_dir : plugin_install_dir
)

lv2_validate = find_program ('lv2_validate', required : false)
if lv2_validate.found()
    test ('lv2_validate', lv2_validate, args : [ manifest_ttl, everb_ttl ])
endif
