/**
 * Real Time Protocol Music Instrument Digital Interface Daemon
 * Copyright (C) 2019-2021 David Moreno Montero <dmoreno@coralbits.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
#include <functional>
#include <jack/jack.h>
#include <libremidi/libremidi.hpp>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <rtpmidid/signal.hpp>

namespace rtpmidid {
class midi_backend {
public:
  struct port_t {
    std::string client;
    std::string port;

    port_t(std::string a, std::string b)
        : client(std::move(a)), port(std::move(b)) {}

    bool operator<(const port_t &other) const {
      return client < other.client && port < other.port;
    }
  };

  struct io_port_t {
    std::unique_ptr<libremidi::midi_in_jack> in_port;
    std::unique_ptr<libremidi::midi_out_jack> out_port;
    std::vector<std::string> subscribers{};

    io_port_t(jack_client_t *client, const std::string &client_name) {
      in_port =
          std::make_unique<libremidi::midi_in_jack>(client_name, 100, client);
      out_port =
          std::make_unique<libremidi::midi_out_jack>(client_name, client);
    }
  };

  std::string client_name;
  jack_client_t *client;
  std::vector<int> fds; // Normally 1?
  std::map<std::string, signal_t<port_t, const std::string>> subscribe_event;
  std::map<std::string, signal_t<port_t>> unsubscribe_event;
  std::map<std::string, signal_t<const libremidi::message &>> midi_event;

  midi_backend(std::string name);
  ~midi_backend();

  void read_ready(const libremidi::message &ev, const std::string &from);

  void create_port(const std::string &name);
  void remove_port(const std::string &name);

  void subscribe_port(const std::string &from, const std::string &to);
  void unsubscribe_port(const std::string &from, const std::string &to);

  // Disconnects everything from this port
  void disconnect_port(const std::string &port);

  void send_midi(const std::string &port, const uint8_t *message, size_t size);

private:
  std::map<std::string, std::unique_ptr<io_port_t>> ports;
};
} // namespace rtpmidid
