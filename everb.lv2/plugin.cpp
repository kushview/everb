// Copyright 2022 Michael Fisher <mfisher@lvtk.org>
// SPDX-License-Identifier: ISC

#include <lvtk/lvtk.hpp>
#include <lvtk/plugin.hpp>

#include <cmath>
#include <cstdint>
#include <vector>

#define EVERB_URI "https://kushview.net/plugins/everb"

struct eVerb : public lvtk::Plugin<eVerb> {

    eVerb (const lvtk::Args& args) : Plugin (args) {}

    void connect_port (uint32_t port, void* data) {
    }

    void run (uint32_t nframes) {
    }
};

static const lvtk::Descriptor<eVerb> volume (EVERB_URI);
