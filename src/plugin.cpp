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

#include <lvtk/memory.hpp>
#include <lvtk/plugin.hpp>

#include "everb.hpp"
#include "ports.hpp"

#define EVERB_URI "https://kushview.net/plugins/everb"

namespace everb{



class Module final : public lvtk::Plugin<Module> {
public:
    Module (const lvtk::Args& args)
        : Plugin (args),
          sampleRate (args.sample_rate),
          bundlePath (args.bundle) {}

    ~Module() {}

    void connect_port (uint32_t port, void* data) {
        switch (port) {
            case Ports::AudioIn_1:
                input[0] = (float*) data;
                break;
            case Ports::AudioIn_2:
                input[1] = (float*) data;
                break;
            case Ports::AudioOut_1:
                output[0] = (float*) data;
                break;
            case Ports::AudioOut_2:
                output[1] = (float*) data;
                break;
        }

        // Lilv will connect NULL on instantiate... just return
        if (data == nullptr)
            return;

        switch (port) {
            case Ports::Wet:
                params.wetLevel = *((float*) data);
                break;
            case Ports::Dry:
                params.dryLevel = *((float*) data);
                break;
            case Ports::RoomSize:
                params.roomSize = *((float*) data);
                break;
            case Ports::Width:
                params.width = *((float*) data);
                break;
            case Ports::Damping:
                params.damping = *((float*) data);
                break;
        }
    }

    void activate() {
        verb.reset();
        verb.setParameters ({});
        verb.setSampleRate (sampleRate);
        params = verb.getParameters();
    }

    void deactivate() {
        // noop
    }

    void run (uint32_t _nframes) noexcept {
        const auto nframes = static_cast<int> (_nframes);

        const auto& vp = verb.getParameters();
        if (vp.damping != params.damping || vp.dryLevel != params.dryLevel || vp.freezeMode != params.freezeMode || vp.roomSize != params.roomSize || vp.wetLevel != params.wetLevel || vp.width != params.width) {
            verb.setParameters (params);
        }

        verb.processStereo (input[0], input[1], output[0], output[1], nframes);
    }

private:
    Reverb verb;
    Reverb::Parameters params, cparams;
    double sampleRate;
    std::string bundlePath;
    float* input[2];
    float* output[2];
};
} // namespace everb

static const lvtk::Descriptor<everb::Module> __everb (EVERB_URI);
