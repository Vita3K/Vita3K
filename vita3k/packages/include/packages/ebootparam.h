// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace packages {

using EbootparamKey = std::array<uint8_t, 32>;
using Ebootparam = std::array<uint8_t, 288>;

// Parses the SELF control-info region and generates ebootparam. Returns no value
// for an invalid SELF, a missing/ambiguous type 6, or a cryptographic failure.
// This does not inspect or require an embedded ELF.
std::optional<Ebootparam> generate_ebootparam(std::span<const uint8_t> self, const EbootparamKey &key);

// Derives the HMAC key from the encrypted portability data; throws if derivation fails.
std::optional<Ebootparam> generate_ebootparam(std::span<const uint8_t> self);

} // namespace packages
