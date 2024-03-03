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

#include <util/vector_math.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace CemuhookUDP {

class Socket;

namespace Response {
enum class Battery : std::uint8_t;
struct PadData;
struct PortInfo;
struct TouchPad;
struct Version;
} // namespace Response

enum class PadTouch {
    Click,
    Undefined,
};

struct UDPPadStatus {
    std::string host{ "127.0.0.1" };
    std::uint16_t port{ 26760 };
    std::size_t pad_index{};
};

struct DeviceStatus {
    std::mutex update_mutex;

    // calibration data for scaling the device's touch area to 3ds
    struct CalibrationData {
        std::uint16_t min_x{};
        std::uint16_t min_y{};
        std::uint16_t max_x{};
        std::uint16_t max_y{};
    };
    std::optional<CalibrationData> touch_calibration;
};

class UDPClient {
public:
    explicit UDPClient(const std::string &address);
    explicit UDPClient()
        : UDPClient("127.0.0.1:26760") {}
    ~UDPClient();

    bool SetAddress(const std::string &address);
    bool IsConnected();
    void SetGyroScale(float f);
    void GetGyroAccel(Util::Vec3f &gyro, uint64_t &gyro_timestamp, Util::Vec3f &accel, uint64_t &accel_timestamp);

private:
    struct PadData {
        std::size_t pad_index{};
        bool connected{};
        DeviceStatus status;
        std::uint64_t packet_sequence{};

        std::chrono::time_point<std::chrono::steady_clock> last_update;
    };

    struct ClientConnection {
        ClientConnection();
        ~ClientConnection();
        std::string host{ "127.0.0.1" };
        std::uint16_t port{ 26760 };
        std::int8_t active{ -1 };
        std::unique_ptr<Socket> socket;
        std::thread thread;
    };

    // For shutting down, clear all data, join all threads, release usb
    void Reset();

    // Translates configuration to client number
    std::size_t GetClientNumber(std::string_view host, std::uint16_t port);

    void OnVersion(Response::Version);
    void OnPortInfo(Response::PortInfo);
    void OnPadData(Response::PadData, std::size_t client);
    void StartCommunication(std::size_t client, const std::string &host, std::uint16_t port);

    // Allocate clients for 8 udp servers
    static constexpr std::size_t MAX_UDP_CLIENTS = 8;
    static constexpr std::size_t PADS_PER_CLIENT = 4;
    std::array<PadData, MAX_UDP_CLIENTS * PADS_PER_CLIENT> pads{};
    std::mutex pads_mutex;
    std::array<ClientConnection, MAX_UDP_CLIENTS> clients{};
    std::mutex clients_mutex;
    std::unique_ptr<Response::PadData> last_pad_data;
    std::mutex last_pad_data_mutex;
    // for fine tuning the sensitivity
    float gyro_scale;
};

} // namespace CemuhookUDP
