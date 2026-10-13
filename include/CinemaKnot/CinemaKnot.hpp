#pragma once

#include "Vector.hpp"
#include "KDTree.hpp"
#include "SignalDescriptors.hpp"
#include "VisualDescriptors.hpp"
#include "Corpus.hpp"
#include "NetworkProtocol.hpp"
#include "UdpSocket.hpp"

namespace CinemaKnot {
    inline const char* getVersion() {
        return "CinemaKnot C++ Core v1.0.0 (Air-Gapped Standalone)";
    }
}
