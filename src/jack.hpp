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
#include <string>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include <rtpmidid/signal.hpp>
#include <rtpmidid/iobytes.hpp>

namespace rtpmidid {
class jack {
public:
  struct port_t {
    std::string client;
    std::string port;

    port_t(std::string& a, std::string& b) : client(a), port(b) {}

//    bool operator<(const port_t &other) const {
//      return client < other.client && port < other.port;
//    }
  };

  struct io_port_t {
    std::string name{};
    jack_port_t *in_port{};
    jack_port_t *out_port{};
  };

  std::string name;
  std::map<std::string, signal_t<port_t, const std::string &>> subscribe_event;
  std::map<std::string, signal_t<port_t>> unsubscribe_event;
  std::map<std::string, signal_t<jack_midi_event_t *>> midi_event;

  explicit jack(std::string name);
  ~jack();

  void create_port(const std::string &name);
  void remove_port(const std::string &name);

  // Disconnects everything from this port
  void disconnect_port(std::string &port);

  void send_midi(const std::string &port_name, io_bytes_reader buffer);
private:
  static const constexpr auto ringbuffer_size = 16384;
  jack_client_t *client;
  std::map<std::string, io_port_t> ports;
  static int process_callback(jack_nframes_t nframes, void *arg);
  jack_ringbuffer_t *out_buffer{};
  jack_ringbuffer_t *size_buffer{};
};

} // namespace rtpmidid
