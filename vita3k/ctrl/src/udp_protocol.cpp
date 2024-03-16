// SPDX-FileCopyrightText: 2018 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Vita3K emulator project
// Copyright (C) 2024 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <ctrl/udp_protocol.h>

#include <util/log.h>

#include <cstddef>
#include <cstring>

namespace CemuhookUDP {

static constexpr std::size_t GetSizeOfResponseType(Type t) {
    switch (t) {
    case Type::Version:
        return sizeof(Response::Version);
    case Type::PortInfo:
        return sizeof(Response::PortInfo);
    case Type::PadData:
        return sizeof(Response::PadData);
    }
    return 0;
}

namespace Response {

/**
 * Returns Type if the packet is valid, else none
 *
 * Note: Modifies the buffer to zero out the crc (since that's the easiest way to check without
 * copying the buffer)
 */
std::optional<Type> Validate(std::uint8_t *data, std::size_t size) {
    if (size < sizeof(Header)) {
        return std::nullopt;
    }
    Header header{};
    std::memcpy(&header, data, sizeof(Header));
    if (header.magic != SERVER_MAGIC) {
        LOG_ERROR("UDP Packet has an unexpected magic value");
        return std::nullopt;
    }
    if (header.protocol_version != PROTOCOL_VERSION) {
        LOG_ERROR("UDP Packet protocol mismatch");
        return std::nullopt;
    }
    if (header.type < Type::Version || header.type > Type::PadData) {
        LOG_ERROR("UDP Packet is an unknown type");
        return std::nullopt;
    }

    // Packet size must equal sizeof(Header) + sizeof(Data)
    // and also verify that the packet info mentions the correct size. Since the spec includes the
    // type of the packet as part of the data, we need to include it in size calculations here
    // ie: payload_length == sizeof(T) + sizeof(Type)
    const std::size_t data_len = GetSizeOfResponseType(header.type);
    if (header.payload_length != data_len + sizeof(Type) || size < data_len + sizeof(Header)) {
        LOG_ERROR(
            "UDP Packet payload length doesn't match. Received: {} PayloadLength: {} Expected: {}",
            size, header.payload_length, data_len + sizeof(Type));
        return std::nullopt;
    }

    const std::uint32_t crc32 = header.crc;
    boost::crc_32_type result;
    // zero out the crc in the buffer and then run the crc against it
    std::memset(&data[offsetof(Header, crc)], 0, sizeof(std::uint32_t));

    result.process_bytes(data, data_len + sizeof(Header));
    if (crc32 != result.checksum()) {
        LOG_ERROR("UDP Packet CRC check failed. Offset: {}", offsetof(Header, crc));
        return std::nullopt;
    }
    return header.type;
}
} // namespace Response

} // namespace CemuhookUDP
