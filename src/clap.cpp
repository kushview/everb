#include <cstring>
#include <memory>

#include <clap/clap.h>
#include <lui/cairo.hpp>

#include "content.hpp"
#include "everb.hpp"
#include "ports.hpp"

struct eVerb {
    clap_plugin_t plugin;
    Reverb reverb;
    Reverb::Parameters params;

    mutable std::mutex params_mutex;

    std::vector<clap_audio_port_info_t> ins, outs;
    std::vector<clap_param_info_t> param_info;

    std::unique_ptr<lvtk::Main> gui;
    std::unique_ptr<Content> content;

    const clap_host_t* host { nullptr };
    const clap_host_timer_support_t* timer { nullptr };
    clap_id idle_timer { CLAP_INVALID_ID };

    double get_param (uint32_t param_id, const Reverb::Parameters& values)
    {
        switch (param_id) {
            case Ports::Damping:
                return values.damping;
                break;
            case Ports::Dry:
                return values.dryLevel;
                break;
            case Ports::RoomSize:
                return values.roomSize;
                break;
            case Ports::Wet:
                return values.wetLevel;
                break;
            case Ports::Width:
                return values.width;
                break;
        }

        return 0.0;
    }

    void sync_params() {
        if (content == nullptr)
            return;
        
        const auto sp = safe_params();
        for (uint32_t port = Ports::paramsBegin(); port < Ports::paramsEnd(); ++port) {
            content->update_slider (port, get_param (port, sp));
        }
    }
    
    Reverb::Parameters safe_params() const noexcept {
        Reverb::Parameters out_params;
        {
            std::lock_guard<std::mutex> sl (params_mutex);
            out_params = params;
        }
        return out_params;
    }

    void update (clap_id param_id, double value) noexcept {
        switch (param_id) {
            case Ports::Damping:
                params.damping = static_cast<float> (value);
                break;
            case Ports::Dry:
                params.dryLevel = static_cast<float> (value);
                break;
            case Ports::RoomSize:
                params.roomSize = static_cast<float> (value);
                break;
            case Ports::Wet:
                params.wetLevel = static_cast<float> (value);
                break;
            case Ports::Width:
                params.width = static_cast<float> (value);
                break;
        }
    }

    void update (const clap_event_param_value_t* value) noexcept {
        update (value->param_id, value->value);
    }

    void apply_params() {
        reverb.setParameters (params);
    }
};

namespace detail {
inline static eVerb& from (const clap_plugin_t* plugin) {
    return *reinterpret_cast<eVerb*> (plugin->plugin_data);
}

inline static void copy_name (char* dst, const char* src) {
    const auto name = juce::String::fromUTF8 (src);
    std::strncpy (dst, name.toRawUTF8(), name.getNumBytesAsUTF8());
    dst[name.getNumBytesAsUTF8()] = '\0';
}

} // namespace detail

static const clap_plugin_audio_ports_t _audio_ports = {
    .count = [] (const clap_plugin_t* plugin, bool is_input) -> uint32_t {
        auto& self = detail::from (plugin);
        auto& vec  = is_input ? self.ins : self.outs;
        return vec.size();
    },

    // Get info about an audio port.
    // Returns true on success and stores the result into info.
    // [main-thread]
    .get = [] (const clap_plugin_t* plugin, uint32_t index, bool is_input,
               clap_audio_port_info_t* info) -> bool {
        auto& self = detail::from (plugin);
        auto& vec  = is_input ? self.ins : self.outs;
        if (index < vec.size()) {
            *info = vec[index];
            return true;
        }
        return false;
    }
};

// Must be called after creating the plugin.
// If init returns false, the host must destroy the plugin instance.
// If init returns true, then the plugin is initialized and in the deactivated state.
// Unlike in `plugin-factory::create_plugin`, in init you have complete access to the host
// and host extensions, so clap related setup activities should be done here rather than in
// create_plugin.
// [main-thread]
static bool everb_init (const clap_plugin_t* plugin) {
    auto& self = detail::from (plugin);

    /* audio ports */
    clap_audio_port_info_t info;
    info.flags         = 0;
    info.id            = 0;
    info.in_place_pair = CLAP_INVALID_ID;
    info.channel_count = 2;
    info.port_type     = CLAP_PORT_STEREO;

    detail::copy_name (info.name, "Input");
    self.ins.push_back (info);
    detail::copy_name (info.name, "Output");
    self.outs.push_back (info);

    /* parameters */
    const Reverb::Parameters defaults;
    self.reverb.reset();
    self.reverb.setParameters (defaults);

    for (uint32_t id = Ports::Wet; id <= Ports::Width; ++id) {
        clap_param_info_t param;
        detail::copy_name (param.module, "Reverb");
        param.cookie    = nullptr;
        param.flags     = 0;
        param.id        = id;
        param.min_value = 0.0;
        param.max_value = 1.0;

        switch (id) {
            case Ports::Wet:
                detail::copy_name (param.name, "Wet");
                param.default_value = defaults.wetLevel;
                break;
            case Ports::Dry:
                detail::copy_name (param.name, "Dry");
                param.default_value = defaults.dryLevel;
                break;
            case Ports::RoomSize:
                detail::copy_name (param.name, "Room Size");
                param.default_value = defaults.roomSize;
                break;
            case Ports::Damping:
                detail::copy_name (param.name, "Damping");
                param.default_value = defaults.damping;
                break;
            case Ports::Width:
                detail::copy_name (param.name, "Width");
                param.default_value = defaults.width;
                break;
        }

        self.param_info.push_back (param);
    }

    if (auto timer = (const clap_host_timer_support_t*) self.host->get_extension (self.host, CLAP_EXT_TIMER_SUPPORT)) {
        self.timer = timer;
    }

    return true;
}

// Free the plugin and its resources.
// It is required to deactivate the plugin prior to this call.
// [main-thread & !active]
static void everb_destroy (const clap_plugin_t* plugin) {
    delete (eVerb*) plugin->plugin_data;
}

// Activate and deactivate the plugin.
// In this call the plugin may allocate memory and prepare everything needed for the process
// call. The process's sample rate will be constant and process's frame count will included in
// the [min, max] range, which is bounded by [1, INT32_MAX].
// In this call the plugin may call host-provided methods marked [being-activated].
// Once activated the latency and port configuration must remain constant, until deactivation.
// Returns true on success.
// [main-thread & !active]
static bool everb_activate (const clap_plugin_t* plugin,
                            double sample_rate,
                            uint32_t min_frames_count,
                            uint32_t max_frames_count) {
    auto& self = detail::from (plugin);
    self.reverb.setSampleRate (sample_rate);
    return true;
}
// [main-thread & active]
static void everb_deactivate (const clap_plugin_t* plugin) {
    juce::ignoreUnused (plugin);
}

// Call start processing before processing.
// Returns true on success.
// [audio-thread & active & !processing]
static bool everb_start_processing (const clap_plugin_t* plugin) {
    juce::ignoreUnused (plugin);
    return true;
}

// Call stop processing before sending the plugin to sleep.
// [audio-thread & active & processing]
static void everb_stop_processing (const clap_plugin_t* plugin) {
    juce::ignoreUnused (plugin);
}

// - Clears all buffers, performs a full reset of the processing state (filters, oscillators,
//   envelopes, lfo, ...) and kills all voices.
// - The parameter's value remain unchanged.
// - clap_process.steady_time may jump backward.
//
// [audio-thread & active]
static void everb_reset (const clap_plugin_t* plugin) {
    detail::from (plugin).reverb.reset();
}

// process audio, events, ...
// All the pointers coming from clap_process_t and its nested attributes,
// are valid until process() returns.
// [audio-thread & active & processing]
static clap_process_status everb_process (const clap_plugin_t* plugin,
                                          const clap_process_t* process) {
    auto& self  = detail::from (plugin);
    auto num_in = process->in_events->size (process->in_events);

    {
        std::lock_guard<std::mutex> sl (self.params_mutex);
        bool param_changed = false;
        for (uint32_t i = 0; i < num_in; ++i) {
            auto ev = process->in_events->get (process->in_events, i);
            switch (ev->type) {
                case CLAP_EVENT_PARAM_VALUE: {
                    auto pv = (const clap_event_param_value_t*) ev;
                    self.update (pv);
                    param_changed = true;
                    break;
                }
            }
        }

        if (param_changed)
            self.apply_params();
    }

    auto& ain  = process->audio_inputs[0];
    auto& aout = process->audio_outputs[0];
    self.reverb.processStereo (ain.data32[0],
                               ain.data32[1],
                               aout.data32[0],
                               aout.data32[1],
                               static_cast<int> (process->frames_count));

    return CLAP_PROCESS_CONTINUE;
}

// Called by the host on the main thread in response to a previous call to:
//   host->request_callback(host);
// [main-thread]
static void everb_on_main_thread (const clap_plugin_t* plugin) {
    juce::ignoreUnused (plugin);
}

//==============================================================================
// Params

#if 0
namespace clap {

class plugin_params {
public:
    using clap_type = clap_plugin_params_t;
    using size_type = size_t;
    plugin_params() {
        init();
    }
    ~plugin_params() {}

    const clap_type* c_obj() const noexcept { return &_params; }
    inline operator const clap_type*() const noexcept { return &_params;}
    constexpr size_type size() const noexcept { return _vec.size(); }

private:
    clap_plugin_params_t _params;
    std::vector<clap_param_info_t> _vec;
    void init() {
        _params.count = &_count;
    }
    static uint32_t _count (const clap_plugin_t* plugin)
};

}
#endif

// Returns the number of parameters.
// [main-thread]
static uint32_t everb_params_count (const clap_plugin_t* plugin) {
    auto& self = detail::from (plugin);
    return self.param_info.size();
}

// Copies the parameter's info to param_info.
// Returns true on success.
// [main-thread]
static bool everb_params_get_info (const clap_plugin_t* plugin,
                                   uint32_t param_index,
                                   clap_param_info_t* param_info) {
    auto& self = detail::from (plugin);
    if (param_index < everb_params_count (plugin)) {
        *param_info = self.param_info[param_index];
        return true;
    }

    return false;
}

// Writes the parameter's current value to out_value.
// Returns true on success.
// [main-thread]
static bool everb_params_get_value (const clap_plugin_t* plugin, clap_id param_id, double* out_value) {
    auto& self = detail::from (plugin);
    for (const auto& param : self.param_info) {
        if (param.id == param_id) {
            const auto vals = self.safe_params();
            switch (param_id) {
                case Ports::Wet:
                    *out_value = vals.wetLevel;
                    break;
                case Ports::Dry:
                    *out_value = vals.dryLevel;
                    break;
                case Ports::RoomSize:
                    *out_value = vals.roomSize;
                    break;
                case Ports::Width:
                    *out_value = vals.width;
                    break;
                case Ports::Damping:
                    *out_value = vals.damping;
                    break;
            }
            return true;
        }
    }
    return false;
}

// Fills out_buffer with a null-terminated UTF-8 string that represents the parameter at the
// given 'value' argument. eg: "2.3 kHz". The host should always use this to format parameter
// values before displaying it to the user.
// Returns true on success.
// [main-thread]
static bool everb_params_value_to_text (const clap_plugin_t* plugin,
                                        clap_id param_id,
                                        double value,
                                        char* out_buffer,
                                        uint32_t out_buffer_capacity) {
    juce::ignoreUnused (plugin, param_id);

    juce::String buf (value);
    auto size = std::min (buf.getNumBytesAsUTF8(), (size_t) out_buffer_capacity);
    std::strncpy (out_buffer, buf.toRawUTF8(), size);
    return true;
}

// Converts the null-terminated UTF-8 param_value_text into a double and writes it to out_value.
// The host can use this to convert user input into a parameter value.
// Returns true on success.
// [main-thread]
static bool everb_params_text_to_value (const clap_plugin_t* plugin,
                                        clap_id param_id,
                                        const char* param_value_text,
                                        double* out_value) {
    juce::ignoreUnused (plugin, param_id);
    const auto str = juce::String::fromUTF8 (param_value_text);
    *out_value     = str.getDoubleValue();
    return true;
}

// Flushes a set of parameter changes.
// This method must not be called concurrently to clap_plugin->process().
//
// Note: if the plugin is processing, then the process() call will already achieve the
// parameter update (bi-directional), so a call to flush isn't required, also be aware
// that the plugin may use the sample offset in process(), while this information would be
// lost within flush().
//
// [active ? audio-thread : main-thread]
static void everb_params_flush (const clap_plugin_t* plugin,
                                const clap_input_events_t* in,
                                const clap_output_events_t* out) {
    juce::ignoreUnused (plugin, in, out);
}

static const clap_plugin_params_t _params = {
    .count         = &everb_params_count,
    .get_info      = &everb_params_get_info,
    .get_value     = &everb_params_get_value,
    .value_to_text = &everb_params_value_to_text,
    .text_to_value = &everb_params_text_to_value,
    .flush         = &everb_params_flush
};

//==============================================================================

// Saves the plugin state into stream.
// Returns true if the state was correctly saved.
// [main-thread]
static bool everb_load (const clap_plugin_t* plugin, const clap_istream_t* stream) {
    auto& self = detail::from (plugin);
    Reverb::Parameters params;
    if (sizeof (params) != stream->read (stream, &params, sizeof (params)))
        return false;

    {std::lock_guard<std::mutex> sl (self.params_mutex);
    self.params = params;
    self.apply_params();}
    self.sync_params();
    return true;
}

// Loads the plugin state from stream.
// Returns true if the state was correctly restored.
// [main-thread]
static bool everb_save (const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    auto& self      = detail::from (plugin);
    auto params     = self.safe_params();
    auto total_size = (int64_t) sizeof (params);
    return total_size == stream->write (stream, &params, total_size);
}

static const clap_plugin_state_t _state = {
    .save = &everb_save,
    .load = &everb_load
};

//==============================================================================

#if __APPLE__
#    define EVERB_WINDOW_API CLAP_WINDOW_API_COCOA
#elif defined(__WIN32__)
#    define EVERB_WINDOW_API CLAP_WINDOW_API_WIN32
#else
#    define EVERB_WINDOW_API CLAP_WINDOW_API_X11
#endif

// Returns true if the requested gui api is supported, either in floating (plugin-created)
// or non-floating (embedded) mode.
// [main-thread]
static bool everb_ui_is_api_supported (const clap_plugin_t* plugin,
                                       const char* api,
                                       bool is_floating) {
    auto& self = detail::from (plugin);
    return ! is_floating && self.timer != nullptr && 0 == std::strcmp (api, EVERB_WINDOW_API);
}

// Returns true if the plugin has a preferred api.
// The host has no obligation to honor the plugin preference, this is just a hint.
// The const char **api variable should be explicitly assigned as a pointer to
// one of the CLAP_WINDOW_API_ constants defined above, not strcopied.
// [main-thread]
static bool everb_ui_get_preferred_api (const clap_plugin_t*,
                                        const char** api,
                                        bool* is_floating) {
    *is_floating = false;
    *api         = EVERB_WINDOW_API;
    return true;
}

// Create and allocate all resources necessary for the gui.
//
// If is_floating is true, then the window will not be managed by the host. The plugin
// can set its window to stays above the parent window, see set_transient().
// api may be null or blank for floating window.
//
// If is_floating is false, then the plugin has to embed its window into the parent window, see
// set_parent().
//
// After this call, the GUI may not be visible yet; don't forget to call show().
//
// Returns true if the GUI is successfully created.
// [main-thread]
static bool everb_ui_create (const clap_plugin_t* plugin, const char* api, bool is_floating) {
    auto& self = detail::from (plugin);

    if (self.content == nullptr) {
        self.gui     = std::make_unique<lvtk::Main> (lvtk::Mode::MODULE, std::make_unique<lvtk::Cairo>());
        self.content = std::make_unique<Content>();
        self.content->on_control_changed = [&](uint32_t port, float value ) {
            std::lock_guard<std::mutex> sl (self.params_mutex);
            self.update (port, value);
            self.apply_params();
        };
        self.timer->register_timer (self.host, 20, &self.idle_timer);
    }

    return true;
}

// Free all resources associated with the gui.
// [main-thread]
static void everb_ui_destroy (const clap_plugin_t* plugin) {
    auto& self = detail::from (plugin);

    if (self.idle_timer != CLAP_INVALID_ID) {
        self.timer->unregister_timer (self.host, self.idle_timer);
        self.idle_timer = CLAP_INVALID_ID;
    }

    self.content.reset();
    self.gui.reset();
}

// Set the absolute GUI scaling factor, and override any OS info.
// Should not be used if the windowing api relies upon logical pixels.
//
// If the plugin prefers to work out the scaling factor itself by querying the OS directly,
// then ignore the call.
//
// scale = 2 means 200% scaling.
//
// Returns true if the scaling could be applied
// Returns false if the call was ignored, or the scaling could not be applied.
// [main-thread]
static bool everb_ui_set_scale (const clap_plugin_t* plugin, double scale) {
    juce::ignoreUnused (plugin, scale);
    return false;
}

// Get the current size of the plugin UI.
// clap_plugin_gui->create() must have been called prior to asking the size.
//
// Returns true if the plugin could get the size.
// [main-thread]
static bool everb_ui_get_size (const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
    auto& self = detail::from (plugin);
    *width     = (uint32_t) self.content->width();
    *height    = (uint32_t) self.content->height();
    return true;
}

// Returns true if the window is resizeable (mouse drag).
// [main-thread & !floating]
static bool everb_ui_can_resize (const clap_plugin_t*) {
    return false;
}

// Returns true if the plugin can provide hints on how to resize the window.
// [main-thread & !floating]
static bool everb_ui_get_resize_hints (const clap_plugin_t* plugin, clap_gui_resize_hints_t* hints) {
    return false;
}

// If the plugin gui is resizable, then the plugin will calculate the closest
// usable size which fits in the given size.
// This method does not change the size.
//
// Returns true if the plugin could adjust the given size.
// [main-thread & !floating]
static bool everb_ui_adjust_size (const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
    return false;
}

// Sets the window size.
//
// Returns true if the plugin could resize its window to the given size.
// [main-thread & !floating]
static bool everb_set_size (const clap_plugin_t* plugin, uint32_t width, uint32_t height) {
    return false;
}

// Embeds the plugin window into the given window.
//
// Returns true on success.
// [main-thread & !floating]
static bool everb_set_parent (const clap_plugin_t* plugin, const clap_window_t* window) {
    auto& self = detail::from (plugin);
    return nullptr != self.gui->elevate (*self.content, 0, window->x11);
}

// Set the plugin floating window to stay above the given window.
//
// Returns true on success.
// [main-thread & floating]
static bool everb_set_transient (const clap_plugin_t* plugin, const clap_window_t* window) {
    return false;
}

// Suggests a window title. Only for floating windows.
//
// [main-thread & floating]
static void everb_suggest_title (const clap_plugin_t* plugin, const char* title) {}

// Show the window.
//
// Returns true on success.
// [main-thread]
static bool everb_show (const clap_plugin_t* plugin) {
    auto& self    = detail::from (plugin);
    auto& content = *self.content;
    self.sync_params();
    content.set_visible (true);
    return true;
}

// Hide the window, this method does not free the resources, it just hides
// the window content. Yet it may be a good idea to stop painting timers.
//
// Returns true on success.
// [main-thread]
static bool everb_hide (const clap_plugin_t* plugin) {
    auto& self    = detail::from (plugin);
    auto& content = *self.content;
    content.set_visible (false);
    return true;
}

static const clap_plugin_gui_t _gui = {
    .is_api_supported  = &everb_ui_is_api_supported,
    .get_preferred_api = &everb_ui_get_preferred_api,
    .create            = &everb_ui_create,
    .destroy           = &everb_ui_destroy,
    .set_scale         = &everb_ui_set_scale,
    .get_size          = &everb_ui_get_size,
    .can_resize        = &everb_ui_can_resize,
    .get_resize_hints  = &everb_ui_get_resize_hints,
    .adjust_size       = &everb_ui_adjust_size,
    .set_size          = &everb_set_size,
    .set_parent        = &everb_set_parent,
    .set_transient     = &everb_set_transient,
    .suggest_title     = &everb_suggest_title,
    .show              = &everb_show,
    .hide              = &everb_hide
};

//==============================================================================
static void everb_on_timer (const clap_plugin_t* plugin, clap_id timer_id) {
    juce::ignoreUnused (timer_id);
    auto& self = detail::from (plugin);
    if (self.gui == nullptr)
        return;
    
    if (timer_id == self.idle_timer)
        self.gui->loop (0.0);
}

static const clap_plugin_timer_support_t _timer {
    .on_timer = &everb_on_timer
};

//==============================================================================
static const clap_plugin_descriptor_t _everb = {
    .clap_version = CLAP_VERSION,
    .id           = "net.kushview.everb",
    .name         = "eVerb",
    .vendor       = "Kushview",
    .url          = "https://github.com/kushview/everb",
    .manual_url   = "https://github.com/kushview/everb",
    .support_url  = "https://github.com/kushview/everb",
    .version      = "1.0.2",
    .description  = "A very simple reverb that uses juce::Reverb",
    .features     = { nullptr }
};

// Query an extension.
// The returned pointer is owned by the plugin.
// It is forbidden to call it before plugin->init().
// You can call it within plugin->init() call, and after.
// [thread-safe]
static const void* everb_get_extension (const clap_plugin_t*, const char* id) {
    if (0 == std::strcmp (id, CLAP_EXT_AUDIO_PORTS)) {
        return &_audio_ports;
    } else if (0 == std::strcmp (id, CLAP_EXT_PARAMS)) {
        return &_params;
    } else if (0 == std::strcmp (id, CLAP_EXT_STATE)) {
        return &_state;
    } else if (0 == std::strcmp (id, CLAP_EXT_GUI)) {
        return &_gui;
    } else if (0 == std::strcmp (id, CLAP_EXT_TIMER_SUPPORT)) {
        return &_timer;
    }
    return nullptr;
}

static const clap_plugin_factory_t _factory = {
    .get_plugin_count = [] (const clap_plugin_factory_t* factory) -> uint32_t {
        return 1;
    },

    // Retrieves a plugin descriptor by its index.
    // Returns null in case of error.
    // The descriptor must not be freed.
    // [thread-safe]
    .get_plugin_descriptor = [] (const clap_plugin_factory_t* factory, uint32_t index) -> const clap_plugin_descriptor_t* {
        return index == 0 ? &_everb : nullptr;
    },

    // Create a clap_plugin by its plugin_id.
    // The returned pointer must be freed by calling plugin->destroy(plugin);
    // The plugin is not allowed to use the host callbacks in the create method.
    // Returns null in case of error.
    // [thread-safe]
    .create_plugin = [] (const clap_plugin_factory_t* factory, const clap_host_t* host, const char* plugin_id) -> const clap_plugin_t* {
        if (factory != &_factory || 0 != std::strcmp (plugin_id, "net.kushview.everb"))
            return nullptr;

        auto verb                = new eVerb();
        verb->host               = host;
        auto plugin              = &verb->plugin;
        plugin->desc             = &_everb;
        plugin->plugin_data      = (void*) verb;
        plugin->activate         = &everb_activate;
        plugin->deactivate       = &everb_deactivate;
        plugin->destroy          = &everb_destroy;
        plugin->get_extension    = &everb_get_extension;
        plugin->init             = &everb_init;
        plugin->on_main_thread   = &everb_on_main_thread;
        plugin->process          = &everb_process;
        plugin->reset            = &everb_reset;
        plugin->start_processing = &everb_start_processing;
        plugin->stop_processing  = &everb_stop_processing;

        return plugin;
    }
};

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    .clap_version = CLAP_VERSION,

    .init = [] (const char* plugin_path) -> bool {
        return true;
    },

    .deinit = []() -> void {},

    .get_factory = [] (const char* factory_id) -> const void* {
        if (0 == std::strcmp (factory_id, CLAP_PLUGIN_FACTORY_ID))
            return (const void*) &_factory;
        return nullptr;
    }
};
