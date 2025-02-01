/*
    This file is part of eVerb

    Copyright (C) 2015-2025  Kushview, LLC.  All rights reserved.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <iostream>

#include <lui/button.hpp>
#include <lui/cairo.hpp>
#include <lui/slider.hpp>
#include <lui/widget.hpp>

#include <lvtk/memory.hpp>
#include <lvtk/ui.hpp>
#include <lvtk/ext/idle.hpp>
#include <lvtk/ext/parent.hpp>
#include <lvtk/ext/resize.hpp>
#include <lvtk/ext/urid.hpp>

#include <lvtk/options.hpp>

#include "content.hpp"
#include "ports.hpp"

#define EVERB_UI_URI "https://kushview.net/plugins/everb/ui"


struct ScopedFlag {
    ScopedFlag (bool& val, bool set) : original (val), value (val) {
        value = set;
    }
    ~ScopedFlag() { value = original; }

private:
    const bool original;
    bool& value;
};

class eVerbUI final : public lvtk::UI<eVerbUI, lvtk::Parent, lvtk::Idle, lvtk::URID, lvtk::Options> {
public:
    eVerbUI (const lvtk::UIArgs& args)
        : UI (args),
          _main (lui::Mode::MODULE, std::make_unique<lui::Cairo>()) {
        for (const auto& opt : lvtk::OptionArray (options())) {
            lvtk::ignore (opt);
        }

        widget();
    }

    void cleanup() {
        content.reset();
    }

    int idle() {
        _main.loop (0);
        return 0;
    }

    bool _block_sending { false };

    void send_control (uint32_t port, float value) {
        if (_block_sending)
            return;
        write (port, value);
    }

    void port_event (uint32_t port, uint32_t size, uint32_t format, const void* buffer) {
        if (format != 0 || size != sizeof (float))
            return;

        ScopedFlag sf (_block_sending, true);
        content->update_slider ((int) port, lvtk::read_unaligned<float> (buffer));
    }

    LV2UI_Widget widget() {
        if (content == nullptr) {
            content = std::make_unique<Content>();
            _main.elevate (*content, 0, (uintptr_t) parent.get());
            content->set_visible (true);
            content->on_control_changed = std::bind (
                &eVerbUI::send_control, this, std::placeholders::_1, std::placeholders::_2);
        }

        return (LV2UI_Widget) content->find_handle();
    }

private:
    lui::Main _main;
    std::unique_ptr<Content> content;
};

static lvtk::UIDescriptor<eVerbUI> __eVerbUI (
    EVERB_UI_URI, { LV2_UI__parent });
