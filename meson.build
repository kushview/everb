project ('everb', ['c', 'cpp'], 
    version : '1.0.0',
    default_options : [ 'cpp_std=c++17' ])

juce_proj = subproject ('juce', default_options: [])
juce_dep = juce_proj.get_variable ('juce_dep')

lvtk_proj = subproject ('lvtk', 
    default_options: [
        'demo=disabled',
        'bindings=disabled',
        'test=disabled',
        'doc=disabled',
        'plugins=disabled',
        'default_library=static'
    ]
)

libeverb_static = static_library ('everb-static', '''
    everb.cpp
'''.split())

subdir ('everb.lv2')
