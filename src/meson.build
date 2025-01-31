fs = import ('fs')

lv2_bundle = 'everb.lv2'
everb_sources = files ('''
    plugin.cpp
'''.split())

if host_machine.system() == 'darwin'
    everb_sources += 'everb.mm'
else
    everb_sources += 'everb.cpp'
endif

everb_ui_type = 'X11UI'
if host_machine.system() == 'windows'
    everb_ui_type = 'WindowsUI'
elif host_machine.system() == 'darwin'
    everb_ui_type = 'CocoaUI'
endif


lv2_install_dir = get_option ('lv2dir').strip()
if lv2_install_dir != ''
    lv2_install_dir = fs.expanduser (lv2_install_dir)
endif

if lv2_install_dir == ''
    if target_machine.system() == 'darwin'
        lv2_install_dir = fs.expanduser ('~/Library/Audio/Plug-Ins/LV2')
    elif target_machine.system() == 'haiku'
        lv2_install_dir = '/boot/common/add-ons/lv2'
    elif target_machine.system() == 'windows'
        lv2_install_dir = fs.expanduser ('~/AppData/Roaming/LV2')
    else
        lv2_install_dir = fs.expanduser('~/.lv2')
    endif
endif

lv2_install_dir = lv2_install_dir / lv2_bundle

plugin = shared_module ('everb',
    everb_sources,
    name_prefix : '',
    dependencies : [ lvtk_dep, juce_dep, clap_dep ],
    install : true,
    install_dir : lv2_install_dir,
    link_args : [ nodelete_cpp_link_args ],
    gnu_symbol_visibility : 'hidden'
)

ui = shared_module ('everb-ui',
    'ui.cpp',
    name_prefix : '',
    dependencies : [ lvtk_cairo_dep ],
    include_directories : [ '.' ],
    install : true,
    install_dir : lv2_install_dir,
    link_args : [ nodelete_cpp_link_args ],
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
    install_dir : lv2_install_dir
)

everb_ttl = configure_file (
    input : 'everb.ttl.in',
    output : 'everb.ttl',
    copy : true,
    install : true,
    install_dir : lv2_install_dir
)

lv2_validate = find_program ('lv2_validate', required : false)
if lv2_validate.found()
    test ('lv2_validate', lv2_validate, args : [ manifest_ttl, everb_ttl ])
endif

# Clap
clap_install_dir = '' # get_option ('clapdir').strip()
if clap_install_dir != ''
    clap_install_dir = fs.expanduser (clap_install_dir)
endif

if clap_install_dir == ''
    if target_machine.system() == 'darwin'
        clap_install_dir = fs.expanduser ('~/Library/Audio/Plug-Ins/CLAP')
    elif target_machine.system() == 'haiku'
        clap_install_dir = '/boot/common/add-ons/clap'
    elif target_machine.system() == 'windows'
        clap_install_dir = fs.expanduser ('~/AppData/Roaming/CLAP')
    else
        clap_install_dir = fs.expanduser('~/.clap')
    endif
endif

clap_plugin = shared_module ('everb',
    [everb_sources, 'clap.cpp'],
    name_prefix : '',
    name_suffix : 'clap',
    dependencies : [ lvtk_dep, juce_dep, clap_dep, lvtk_cairo_dep ],
    install : true,
    install_dir : clap_install_dir,
    link_args : [ nodelete_cpp_link_args ],
    gnu_symbol_visibility : 'hidden'
)
