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
#include "./jack.hpp"
#include <fmt/format.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>
#include <rtpmidid/exceptions.hpp>
#include <rtpmidid/logger.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/rtpclient.hpp>
#include <stdio.h>

namespace rtpmidid {

jack::jack(std::string _name) : name(std::move(_name)) {
  client = jack_client_open(name.c_str(), JackNoStartServer, nullptr);
  if (client == nullptr) {
    ERROR("Failed to open Jack client.");
  }

  //        INFO("New Jack connection from {} to {}",
  //             jack_port_name(jack_port_by_id(client, a)),
  //             jack_port_name(jack_port_by_id(client, a)));
  jack_set_port_connect_callback(
      client,
      [](jack_port_id_t a, jack_port_id_t b, int c, void *arg) {
        if (c == 1) {
          INFO("New Jack connection from {} to {}",
               jack_port_name(
                   jack_port_by_id(static_cast<jack_client_t *>(arg), a)),
               jack_port_name(
                   jack_port_by_id(static_cast<jack_client_t *>(arg), b)));
        } else {
          INFO("Removed Jack connection from {} to {}",
               jack_port_name(
                   jack_port_by_id(static_cast<jack_client_t *>(arg), a)),
               jack_port_name(
                   jack_port_by_id(static_cast<jack_client_t *>(arg), b)));
        }
      },
      client);

  jack_set_process_callback(client, process_callback, &ports);

  auto result = jack_activate(client);
  if (result != 0) {
    fprintf(stderr, "Could not activate client.\n");
    exit(EXIT_FAILURE);
  }
}

jack::~jack() {
  if (client != nullptr) {
    jack_client_close(client);
  }
}

void jack::create_port(const std::string &name) {
  //  port.first = jack_port_register (client, fmt::format("name. in").c_str(),
  //  JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  io_port_t port;
  port.name = name;
  port.in_port = jack_port_register(client, fmt::format("{} in", name).c_str(),
                                    JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  //  port.second = jack_port_register (client, fmt::format("name.
  //  out").c_str(), JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  port.out_port =
      jack_port_register(client, fmt::format("{} out", name).c_str(),
                         JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  if (port.in_port == nullptr || port.out_port == nullptr) {
    ERROR("Failed to create Jack ports for {}", name);
  }
  ports[name] = std::move(port);
}

void jack::remove_port(const std::string &name) {
  if (ports.find(name) == ports.end()) {
    ERROR("Jack port {} does not exist, so cannot remove.");
    return;
  }
  jack_port_unregister(client, ports[name].in_port);
  jack_port_unregister(client, ports[name].out_port);
}

void jack::disconnect_port(std::string &port) {
  DEBUG("Disconnect Jack port {}", port);
  auto jack_port = jack_port_by_name(client, port.c_str());
  if (jack_port_get_connections(jack_port))
    jack_port_disconnect(client, jack_port);
  else
    ERROR("Jack got command to disconnect {} but not connected.", port);
}

int jack::process_callback(jack_nframes_t nframes, void *arg) {
  auto ports = static_cast<std::map<std::string, io_port_t> *>(arg);
  jack_midi_event_t ev;
  for (const auto& i: *ports) {
    auto buffer = jack_port_get_buffer(i.second.in_port, nframes);
    jack_nframes_t event_count = jack_midi_get_event_count(buffer);
    if (event_count > 0) {
      for (auto i = 0; i < event_count; ++i) {
        jack_midi_event_get(&ev, buffer, i);
        DEBUG("Jack MIDI event received. port: {}, event: {}");
      }
    }
  }
  return 0;
}
} // namespace rtpmidid
