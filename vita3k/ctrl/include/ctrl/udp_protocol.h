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

#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <type_traits>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4701) // Potentially uninitialized local variable 'result' used
#endif

#include <boost/crc.hpp>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace CemuhookUDP {

constexpr std::size_t MAX_PACKET_SIZE = 100;
constexpr std::uint16_t PROTOCOL_VERSION = 1001;
constexpr std::uint32_t CLIENT_MAGIC = 0x43555344; // DSUC (but flipped for LE)
constexpr std::uint32_t SERVER_MAGIC = 0x53555344; // DSUS (but flipped for LE)

enum class Type : std::uint32_t {
    Version = 0x00100000,
    PortInfo = 0x00100001,
    PadData = 0x00100002,
};

struct Header {
    std::uint32_t magic{};
    std::uint16_t protocol_version{};
    std::uint16_t payload_length{};
    std::uint32_t crc{};
    std::uint32_t id{};
    ///> In the protocol, the type of the packet is not part of the header, but its convenient to
    ///> include in the header so the callee doesn't have to duplicate the type twice when building
    ///> the data
    Type type{};
};
static_assert(sizeof(Header) == 20, "UDP Message Header struct has wrong size");
static_assert(std::is_trivially_copyable_v<Header>, "UDP Message Header is not trivially copyable");

using MacAddress = std::array<std::uint8_t, 6>;
constexpr MacAddress EMPTY_MAC_ADDRESS = { 0, 0, 0, 0, 0, 0 };

#pragma pack(push, 1)
template <typename T>
struct Message {
    Header header{};
    T data;
};
#pragma pack(pop)

template <typename T>
constexpr Type GetMessageType();

template <typename T>
Message<T> CreateMessage(const std::uint32_t magic, const T data, const std::uint32_t sender_id) {
    boost::crc_32_type crc;
    Header header{
        magic,
        PROTOCOL_VERSION,
        sizeof(T) + sizeof(Type),
        0,
        sender_id,
        GetMessageType<T>(),
    };
    Message<T> message{ header, data };
    crc.process_bytes(&message, sizeof(Message<T>));
    message.header.crc = crc.checksum();
    return message;
}

namespace Request {

enum RegisterFlags : std::uint8_t {
    AllPads,
    PadID,
    PadMACAddress,
};

struct Version {};
/**
 * Requests the server to send information about what controllers are plugged into the ports
 * In yuzu's case, we only have one controller, so for simplicity's sake, we can just send a
 * request explicitly for the first controller port and leave it at that. In the future it would be
 * nice to make this configurable
 */
constexpr std::uint32_t MAX_PORTS = 4;
struct PortInfo {
    std::uint32_t pad_count{}; ///> Number of ports to request data for
    std::array<std::uint8_t, MAX_PORTS> port;
};
static_assert(std::is_trivially_copyable_v<PortInfo>,
    "UDP Request PortInfo is not trivially copyable");

/**
 * Request the latest pad information from the server. If the server hasn't received this message
 * from the client in a reasonable time frame, the server will stop sending updates. The default
 * timeout seems to be 5 seconds.
 */
struct PadData {
    /// Determines which method will be used as a look up for the controller
    RegisterFlags flags{};
    /// Index of the port of the controller to retrieve data about
    std::uint8_t port_id{};
    /// Mac address of the controller to retrieve data about
    MacAddress mac;
};
static_assert(sizeof(PadData) == 8, "UDP Request PadData struct has wrong size");
static_assert(std::is_trivially_copyable_v<PadData>,
    "UDP Request PadData is not trivially copyable");

/**
 * Creates a message with the proper header data that can be sent to the server.
 * @param data Request body to send
 * @param client_id ID of the udp client (usually not checked on the server)
 */
template <typename T>
Message<T> Create(const T data, const std::uint32_t client_id = 0) {
    return CreateMessage(CLIENT_MAGIC, data, client_id);
}
} // namespace Request

namespace Response {

enum class ConnectionType : std::uint8_t {
    None,
    Usb,
    Bluetooth,
};

enum class State : std::uint8_t {
    Disconnected,
    Reserved,
    Connected,
};

enum class Model : std::uint8_t {
    None,
    PartialGyro,
    FullGyro,
    Generic,
};

enum class Battery : std::uint8_t {
    None = 0x00,
    Dying = 0x01,
    Low = 0x02,
    Medium = 0x03,
    High = 0x04,
    Full = 0x05,
    Charging = 0xEE,
    Charged = 0xEF,
};

struct Version {
    std::uint16_t version{};
};
static_assert(sizeof(Version) == 2, "UDP Response Version struct has wrong size");
static_assert(std::is_trivially_copyable_v<Version>,
    "UDP Response Version is not trivially copyable");

struct PortInfo {
    std::uint8_t id{};
    State state{};
    Model model{};
    ConnectionType connection_type{};
    MacAddress mac;
    Battery battery{};
    std::uint8_t is_pad_active{};
};
static_assert(sizeof(PortInfo) == 12, "UDP Response PortInfo struct has wrong size");
static_assert(std::is_trivially_copyable_v<PortInfo>,
    "UDP Response PortInfo is not trivially copyable");

struct TouchPad {
    std::uint8_t is_active{};
    std::uint8_t id{};
    std::uint16_t x{};
    std::uint16_t y{};
};
static_assert(sizeof(TouchPad) == 6, "UDP Response TouchPad struct has wrong size ");

#pragma pack(push, 1)
struct PadData {
    PortInfo info{};
    std::uint32_t packet_counter{};

    std::uint16_t digital_button{};
    // The following union isn't trivially copyable but we don't use this input anyway.
    // union DigitalButton {
    //     std::uint16_t button;
    //     BitField<0, 1, std::uint16_t> button_1;   // Share
    //     BitField<1, 1, std::uint16_t> button_2;   // L3
    //     BitField<2, 1, std::uint16_t> button_3;   // R3
    //     BitField<3, 1, std::uint16_t> button_4;   // Options
    //     BitField<4, 1, std::uint16_t> button_5;   // Up
    //     BitField<5, 1, std::uint16_t> button_6;   // Right
    //     BitField<6, 1, std::uint16_t> button_7;   // Down
    //     BitField<7, 1, std::uint16_t> button_8;   // Left
    //     BitField<8, 1, std::uint16_t> button_9;   // L2
    //     BitField<9, 1, std::uint16_t> button_10;  // R2
    //     BitField<10, 1, std::uint16_t> button_11; // L1
    //     BitField<11, 1, std::uint16_t> button_12; // R1
    //     BitField<12, 1, std::uint16_t> button_13; // Triangle
    //     BitField<13, 1, std::uint16_t> button_14; // Circle
    //     BitField<14, 1, std::uint16_t> button_15; // Cross
    //     BitField<15, 1, std::uint16_t> button_16; // Square
    // } digital_button;

    std::uint8_t home;
    /// If the device supports a "click" on the touchpad, this will change to 1 when a click happens
    std::uint8_t touch_hard_press{};
    std::uint8_t left_stick_x{};
    std::uint8_t left_stick_y{};
    std::uint8_t right_stick_x{};
    std::uint8_t right_stick_y{};

    struct AnalogButton {
        std::uint8_t button_dpad_left_analog{};
        std::uint8_t button_dpad_down_analog{};
        std::uint8_t button_dpad_right_analog{};
        std::uint8_t button_dpad_up_analog{};
        std::uint8_t button_square_analog{};
        std::uint8_t button_cross_analog{};
        std::uint8_t button_circle_analog{};
        std::uint8_t button_triangle_analog{};
        std::uint8_t button_r1_analog{};
        std::uint8_t button_l1_analog{};
        std::uint8_t trigger_r2{};
        std::uint8_t trigger_l2{};
    } analog_button;

    std::array<TouchPad, 2> touch;

    std::uint64_t motion_timestamp;

    struct Accelerometer {
        float x{};
        float y{};
        float z{};
    } accel;

    struct Gyroscope {
        float pitch{};
        float yaw{};
        float roll{};
    } gyro;
};
#pragma pack(pop)

static_assert(sizeof(PadData) == 80, "UDP Response PadData struct has wrong size ");
static_assert(std::is_trivially_copyable_v<PadData>,
    "UDP Response PadData is not trivially copyable");

static_assert(sizeof(Message<PadData>) == MAX_PACKET_SIZE,
    "UDP MAX_PACKET_SIZE is no longer larger than Message<PadData>");

static_assert(sizeof(PadData::AnalogButton) == 12,
    "UDP Response AnalogButton struct has wrong size ");
static_assert(sizeof(PadData::Accelerometer) == 12,
    "UDP Response Accelerometer struct has wrong size ");
static_assert(sizeof(PadData::Gyroscope) == 12, "UDP Response Gyroscope struct has wrong size ");

/**
 * Create a Response Message from the data
 * @param data array of bytes sent from the server
 * @return boost::none if it failed to parse or Type if it succeeded. The client can then safely
 * copy the data into the appropriate struct for that Type
 */
std::optional<Type> Validate(std::uint8_t *data, std::size_t size);

} // namespace Response

template <>
constexpr Type GetMessageType<Request::Version>() {
    return Type::Version;
}
template <>
constexpr Type GetMessageType<Request::PortInfo>() {
    return Type::PortInfo;
}
template <>
constexpr Type GetMessageType<Request::PadData>() {
    return Type::PadData;
}
template <>
constexpr Type GetMessageType<Response::Version>() {
    return Type::Version;
}
template <>
constexpr Type GetMessageType<Response::PortInfo>() {
    return Type::PortInfo;
}
template <>
constexpr Type GetMessageType<Response::PadData>() {
    return Type::PadData;
}
} // namespace CemuhookUDP
