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
//#include <functional>
//#include <map>
#include <string>
//#include <vector>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#include <rtpmidid/signal.hpp>

namespace rtpmidid {
class jack {
public:
  struct io_port_t {
    std::string client;
    std::string name;
    jack_port_t *in_port;
    jack_port_t *out_port;
    jack_ringbuffer_t *in_buffer;
    jack_ringbuffer_t *out_buffer;
  };

  std::string name;
  std::map<std::string, signal_t<io_port_t &, const std::string &>> subscribe_event;
  std::map<std::string, signal_t<io_port_t &>> unsubscribe_event;
  std::map<std::string, signal_t<jack_midi_event_t *>> midi_event;

  explicit jack(std::string name);
  ~jack();

  void read_ready();

  void create_port(const std::string &name);
  void remove_port(const std::string &name);

  /// Disconencts everything from this port
  void disconnect_port(std::string &port);
private:
  jack_client_t *client;
//  std::map<std::string, std::pair<jack_port_t*,jack_port_t*>> ports;
  std::map<std::string, io_port_t> ports;
};

} // namespace rtpmidid
