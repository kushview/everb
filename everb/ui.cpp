/*
    This file is part of Roboverb

    Copyright (C) 2015-2023  Kushview, LLC.  All rights reserved.

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

#include <lvtk/memory.hpp>

#include <lvtk/ui.hpp>
#include <lvtk/ui/button.hpp>
#include <lvtk/ui/cairo.hpp>
#include <lvtk/ui/slider.hpp>
#include <lvtk/ui/widget.hpp>

#include <lvtk/ext/idle.hpp>
#include <lvtk/ext/parent.hpp>
#include <lvtk/ext/resize.hpp>
#include <lvtk/ext/urid.hpp>

#include <lvtk/options.hpp>

#include "ports.hpp"

#define EVERB_UI_URI "https://kushview.net/plugins/everb/ui"

using Slider = lvtk::Slider;

class ControlLabel : public lvtk::Widget {
public:
    ControlLabel (const std::string& text) {
        set_name (text);
        _text = name();
    }

    void paint (lvtk::Graphics& g) override {
        g.set_color (0xffffffff);
        g.set_font (lvtk::Font (11.f));
        g.draw_text (_text, bounds().at (0).as<float>(), _align);
    }

    void set_text (const std::string& text) {
        _text = text;
        repaint();
    }

private:
    lvtk::Justify _align { lvtk::Justify::CENTERED };
    std::string _text;
};

class Content : public lvtk::Widget {
public:
    std::function<void (uint32_t, float)> on_control_changed;

    Content() {
        set_opaque (true);

        for (int i = Ports::Wet; i <= Ports::Width; ++i) {
            auto s = add (new lvtk::Dial());
            s->set_range (0.0, 1.0);
            // s->set_range ()
            // s->set_type (Slider::HORIZONTAL_BAR);

            s->on_value_changed = [&, i, s]() {
                if (on_control_changed) {
                    const auto port  = static_cast<uint32_t> (i);
                    const auto value = static_cast<float> (s->value());
                    on_control_changed (port, value);
                }
            };

            sliders.push_back (s);

            std::string text = "";
            switch (i) {
                case Ports::Wet:
                    text = "Wet level";
                    break;
                case Ports::Dry:
                    text = "Dry level";
                    break;
                case Ports::RoomSize:
                    text = "Room size";
                    break;
                case Ports::Damping:
                    text = "Damping";
                    break;
                case Ports::Width:
                    text = "Width";
                    break;
            }

            labels.push_back (add (new ControlLabel (text)));
        }

        show_all();
        set_size (int (640 * 0.72), int(360 * 0.72));
    }

    ~Content() {
        for (auto s : sliders)
            delete s;
        sliders.clear();
    }

    template <typename Ft>
    void update_slider (uint32_t port, Ft value) {
        if (! (port >= Ports::Wet && port <= Ports::Width))
            return;

        auto index  = static_cast<int> (port - Ports::Wet);
        auto dvalue = static_cast<double> (value);
        sliders[index]->set_value (dvalue, lvtk::Notify::NONE);
    }

protected:
    void resized() override {
        auto sb = bounds().at (0);
        sb.slice_top (33);
        int h = sb.width / 5;
        for (int i = 0; i < 5; ++i) {
            auto cr = sb.slice_left (h);
            auto dial = sliders[i];
            auto label = labels[i];
            dial->set_bounds (cr.slice_top (h));
            label->set_bounds (cr.slice_top (24));
        }
    }

    void paint (lvtk::Graphics& g) override {
        g.set_color (0xff242222);
        g.fill_rect (bounds().at (0));
        g.set_color (0xccffffff);
        g.draw_text ("  eVerb",
                     bounds().at (0).smaller (3, 4).as<float>(),
                     lvtk::Justify::TOP_LEFT);
    }

private:
    std::vector<lvtk::Dial*> sliders;
    std::vector<ControlLabel*> labels;
};

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
          _main (lvtk::Mode::MODULE, std::make_unique<lvtk::Cairo>()) {
        for (const auto& opt : lvtk::OptionArray (options())) {
            // if (opt.key == map_uri (LV2_UI__scaleFactor))
            //     m_scale_factor = *(float*) opt.value;
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
    lvtk::Main _main;
    std::unique_ptr<Content> content;
};

static lvtk::UIDescriptor<eVerbUI> __eVerbUI (
    EVERB_UI_URI, { LV2_UI__parent });
