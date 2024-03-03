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

#include <ctrl/udp_client.h>
#include <ctrl/udp_protocol.h>

#include <memory>
#include <mutex>
#include <util/log.h>

#include <boost/asio.hpp>
#include <fmt/format.h>
#include <random>

using boost::asio::ip::udp;

namespace CemuhookUDP {

struct SocketCallback {
    std::function<void(Response::Version)> version;
    std::function<void(Response::PortInfo)> port_info;
    std::function<void(Response::PadData)> pad_data;
};

class Socket {
public:
    using clock = std::chrono::system_clock;

    explicit Socket(const std::string &host, std::uint16_t port, SocketCallback callback_)
        : callback(std::move(callback_))
        , timer(io_service)
        , socket(io_service, udp::endpoint(udp::v4(), 0))
        , client_id(GenerateRandomClientId()) {
        boost::system::error_code ec{};
        auto ipv4 = boost::asio::ip::make_address_v4(host, ec);
        if (ec.value() != boost::system::errc::success) {
            LOG_ERROR("Invalid IPv4 address \"{}\" provided to socket", host);
            ipv4 = boost::asio::ip::address_v4{};
        }

        send_endpoint = { udp::endpoint(ipv4, port) };
    }

    void Stop() {
        io_service.stop();
    }

    void Loop() {
        io_service.run();
    }

    void StartSend(const clock::time_point &from) {
        timer.expires_at(from + std::chrono::seconds(3));
        timer.async_wait([this](const boost::system::error_code &error) { HandleSend(error); });
    }

    void StartReceive() {
        socket.async_receive_from(
            boost::asio::buffer(receive_buffer), receive_endpoint,
            [this](const boost::system::error_code &error, std::size_t bytes_transferred) {
                HandleReceive(error, bytes_transferred);
            });
    }

private:
    std::uint32_t GenerateRandomClientId() const {
        std::random_device device;
        return device();
    }

    void HandleReceive(const boost::system::error_code &, std::size_t bytes_transferred) {
        if (auto type = Response::Validate(receive_buffer.data(), bytes_transferred)) {
            switch (*type) {
            case Type::Version: {
                Response::Version version;
                std::memcpy(&version, &receive_buffer[sizeof(Header)], sizeof(Response::Version));
                callback.version(std::move(version));
                break;
            }
            case Type::PortInfo: {
                Response::PortInfo port_info;
                std::memcpy(&port_info, &receive_buffer[sizeof(Header)],
                    sizeof(Response::PortInfo));
                callback.port_info(std::move(port_info));
                break;
            }
            case Type::PadData: {
                Response::PadData pad_data;
                std::memcpy(&pad_data, &receive_buffer[sizeof(Header)], sizeof(Response::PadData));
                callback.pad_data(std::move(pad_data));
                break;
            }
            }
        }
        StartReceive();
    }

    void HandleSend(const boost::system::error_code &) {
        boost::system::error_code _ignored{};
        // Send a request for getting port info for the pad
        const Request::PortInfo port_info{ 4, { 0, 1, 2, 3 } };
        const auto port_message = Request::Create(port_info, client_id);
        std::memcpy(&send_buffer1, &port_message, PORT_INFO_SIZE);
        socket.send_to(boost::asio::buffer(send_buffer1), send_endpoint, {}, _ignored);

        // Send a request for getting pad data for the pad
        const Request::PadData pad_data{
            Request::RegisterFlags::AllPads,
            0,
            EMPTY_MAC_ADDRESS,
        };
        const auto pad_message = Request::Create(pad_data, client_id);
        std::memcpy(send_buffer2.data(), &pad_message, PAD_DATA_SIZE);
        socket.send_to(boost::asio::buffer(send_buffer2), send_endpoint, {}, _ignored);
        StartSend(timer.expiry());
    }

    SocketCallback callback;
    boost::asio::io_service io_service;
    boost::asio::basic_waitable_timer<clock> timer;
    udp::socket socket;

    const std::uint32_t client_id;

    static constexpr std::size_t PORT_INFO_SIZE = sizeof(Message<Request::PortInfo>);
    static constexpr std::size_t PAD_DATA_SIZE = sizeof(Message<Request::PadData>);
    std::array<std::uint8_t, PORT_INFO_SIZE> send_buffer1;
    std::array<std::uint8_t, PAD_DATA_SIZE> send_buffer2;
    udp::endpoint send_endpoint;

    std::array<std::uint8_t, MAX_PACKET_SIZE> receive_buffer;
    udp::endpoint receive_endpoint;
};

static void SocketLoop(Socket *socket) {
    socket->StartReceive();
    socket->StartSend(Socket::clock::now());
    socket->Loop();
}

UDPClient::UDPClient() {
    LOG_INFO("Udp Initialization started");
    StartCommunication(0, "127.0.0.1", 26760);
}

UDPClient::~UDPClient() {
    Reset();
}

UDPClient::ClientConnection::ClientConnection() = default;

UDPClient::ClientConnection::~ClientConnection() = default;

std::size_t UDPClient::GetClientNumber(std::string_view host, std::uint16_t port) {
    std::lock_guard<std::mutex> guard(clients_mutex);
    for (std::size_t client = 0; client < clients.size(); client++) {
        if (clients[client].active == -1) {
            continue;
        }
        if (clients[client].host == host && clients[client].port == port) {
            return client;
        }
    }
    return MAX_UDP_CLIENTS;
}

void UDPClient::OnVersion([[maybe_unused]] Response::Version data) {
    LOG_TRACE("Version packet received: {}", data.version);
}

void UDPClient::OnPortInfo([[maybe_unused]] Response::PortInfo data) {
    LOG_TRACE("PortInfo packet received: {}", (std::uint8_t)data.model);
}

void UDPClient::OnPadData(Response::PadData data, std::size_t client) {
    const std::size_t pad_index = (client * PADS_PER_CLIENT) + data.info.id;

    std::lock_guard<std::mutex> guard_pads(pads_mutex);
    if (pad_index >= pads.size()) {
        LOG_ERROR("Invalid pad id {}", data.info.id);
        return;
    }

    LOG_TRACE("PadData packet received");
    if (data.packet_counter == pads[pad_index].packet_sequence) {
        LOG_WARN(
            "PadData packet dropped because its stale info. Current count: {} Packet count: {}",
            pads[pad_index].packet_sequence, data.packet_counter);
        pads[pad_index].connected = false;
        return;
    }

    std::lock_guard<std::mutex> guard_clients(clients_mutex);
    clients[client].active = 1;
    pads[pad_index].connected = true;
    pads[pad_index].packet_sequence = data.packet_counter;

    const auto now = std::chrono::steady_clock::now();
    const auto time_difference = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - pads[pad_index].last_update)
            .count());
    pads[pad_index].last_update = now;

    {
        std::lock_guard<std::mutex> guard(last_pad_data_mutex);
        if (!last_pad_data)
            last_pad_data.reset(new Response::PadData{});
        *last_pad_data = data;
    }
}

void UDPClient::StartCommunication(std::size_t client, const std::string &host, std::uint16_t port) {
    SocketCallback callback{ [this](Response::Version version) { OnVersion(version); },
        [this](Response::PortInfo info) { OnPortInfo(info); },
        [this, client](Response::PadData data) { OnPadData(data, client); } };
    LOG_INFO("Starting communication with UDP input server on {}:{}", host, port);
    std::lock_guard<std::mutex> guard_clients(clients_mutex);
    clients[client].host = host;
    clients[client].port = port;
    clients[client].active = 0;
    clients[client].socket = std::make_unique<Socket>(host, port, callback);
    clients[client].thread = std::thread{ SocketLoop, clients[client].socket.get() };
}

void UDPClient::Reset() {
    std::lock_guard<std::mutex> guard_clients(clients_mutex);
    for (auto &client : clients) {
        if (client.thread.joinable()) {
            client.active = -1;
            client.socket->Stop();
            client.thread.join();
        }
    }
}

bool UDPClient::IsConnected() {
    std::lock_guard<std::mutex> guard_pads(pads_mutex);
    for (auto &pad : pads) {
        if (pad.connected)
            return true;
    }
    return false;
}

void UDPClient::GetGyroAccel(Util::Vec3f &gyro, uint64_t &gyro_timestamp, Util::Vec3f &accel, uint64_t &accel_timestamp) {
    std::lock_guard<std::mutex> guard(last_pad_data_mutex);
    if (!last_pad_data)
        return;
    // Gyroscope values are not it the correct scale from better joy.
    // Dividing by 312 allows us to make one full turn = 1 turn
    // This must be a configurable valued called sensitivity
    const float gyro_scale = 1.0f / 312.0f;
    gyro.x = last_pad_data->gyro.pitch * gyro_scale;
    gyro.y = last_pad_data->gyro.roll * gyro_scale;
    gyro.z = last_pad_data->gyro.yaw * gyro_scale;
    gyro_timestamp = last_pad_data->motion_timestamp;
    accel.x = last_pad_data->accel.x;
    accel.y = last_pad_data->accel.y;
    accel.z = last_pad_data->accel.z;
    accel_timestamp = last_pad_data->motion_timestamp;
}

} // namespace CemuhookUDP
