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
#include <fmt/format.h>
#include <rtpmidid/exceptions.hpp>
#include <rtpmidid/logger.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/rtpclient.hpp>
#include <stdio.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>
#include "./jack.hpp"

namespace rtpmidid {

jack::jack(std::string _name) : name(std::move(_name)) {
  client = jack_client_open(name.c_str(), JackNoStartServer, nullptr);
  if (client == nullptr) {
    ERROR("Failed to open Jack client.");
  }

  auto result = jack_activate (client);
  if (result != 0) {
    fprintf (stderr, "Could not activate client.\n");
    exit (EXIT_FAILURE);
  }
}

jack::~jack() {
  if (client != nullptr) {
    jack_client_close(client);
  }
}

void jack::create_port(const std::string &name) {
//  port.first = jack_port_register (client, fmt::format("name. in").c_str(), JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  io_port_t port;
  port.name = name;
  port.in_port = jack_port_register (client, fmt::format("{} in", name).c_str(), JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
//  port.second = jack_port_register (client, fmt::format("name. out").c_str(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  port.out_port = jack_port_register (client, fmt::format("{} out", name).c_str(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  if (port.in_port == nullptr || port.out_port == nullptr) {
    ERROR("Failed to create Jack ports for {}", name);
  }
  ports[name] = std::move(port);
}

void jack::remove_port(const std::string &name) {
  if(ports.find(name) == ports.end()) {
    ERROR("Jack port {} does not exist, so cannot remove.");
    return;
  }
  jack_port_unregister(client, ports[name].in_port);
  jack_port_unregister(client, ports[name].out_port);
}

std::vector<std::string> jack::get_port_names() {
  std::vector<std::string> ret;

  for (const auto& i: ports) {
    ret.push_back(i.second.name);
  }

  return ret;
}

void jack::disconnect_port(std::string &port) {
  DEBUG("Disconnect Jack port {}", port);
}
} // namespace rtpmidid
