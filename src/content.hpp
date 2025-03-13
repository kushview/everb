
#pragma once

#include <string>
#include <vector>

#include <lui/graphics.hpp>
#include <lui/slider.hpp>
#include <lui/widget.hpp>

#include "ports.hpp"

using Slider = lui::Slider;

namespace everb {
class ControlLabel : public lui::Widget {
public:
    ControlLabel (const std::string& text) {
        set_name (text);
        _text = name();
    }

    void paint (lui::Graphics& g) override {
        g.set_color (0xffffffff);
        g.set_font (lui::Font (11.f));
        g.draw_text (_text, bounds().at (0).as<float>(), _align);
    }

    void set_text (const std::string& text) {
        _text = text;
        repaint();
    }

private:
    lui::Justify _align { lui::Justify::CENTERED };
    std::string _text;
};

class Content : public lui::Widget {
public:
    std::function<void (uint32_t, float)> on_control_changed;

    Content() {
        set_opaque (true);

        for (int i = Ports::Wet; i <= Ports::Width; ++i) {
            auto s = add (new lui::Dial());
            s->set_range (0.0, 1.0);

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
        set_size (int (640 * 0.8), 150);
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
        sliders[index]->set_value (dvalue, lui::Notify::NONE);
    }

protected:
    void resized() override {
        auto sb = bounds().at (0);
        sb.slice_top(10);
        int h = sb.width / 5;
        for (int i = 0; i < 5; ++i) {
            auto cr = sb.slice_left (h);
            auto dial = sliders[i];
            auto label = labels[i];
            dial->set_bounds (cr.slice_top (h));
            label->set_bounds (cr.slice_top (24));
        }
    }

    void paint (lui::Graphics& g) override {
        g.set_color (0xff242222);
        g.fill_rect (bounds().at (0));
        g.set_color (0xccffffff);
        g.draw_text ("   eVerb",
                     bounds().at (0).slice_top(24).smaller (3, 4).as<float>(),
                     lui::Justify::MID_LEFT);
    }

private:
    std::vector<lui::Dial*> sliders;
    std::vector<ControlLabel*> labels;
};
} // namespace everb