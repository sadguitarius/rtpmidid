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
#include <rtpmidid/logger.hpp>
#include <rtpmidid/poller.hpp>

namespace rtpmidid {

jack::jack(std::string _name) : name(std::move(_name)) {
  client = jack_client_open(name.c_str(), JackNoStartServer, nullptr);
  if (client == nullptr) {
    ERROR("Failed to open Jack client.");
    return;
  }

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

  jack_set_process_callback(client, process_callback, this);

  jack_activate(client);
}

jack::~jack() {
  for (auto i : ports) {
    remove_port(i.first);
  }

  if (client != nullptr) {
    jack_client_close(client);
  }
}

void jack::create_port(const std::string &name) {
  io_port_t port;
  port.name = name;
  port.in_port = jack_port_register(client, fmt::format("{} in", name).c_str(),
                                    JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
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
  jack_ringbuffer_free(ports[name].out_buffer);
  jack_ringbuffer_free(ports[name].size_buffer);
  ports.erase(name);
}

void jack::disconnect_port(std::string &port) {
  DEBUG("Disconnect Jack port {}", port);
  auto jack_port = jack_port_by_name(client, port.c_str());
  if (jack_port_get_connections(jack_port))
    jack_port_disconnect(client, jack_port);
  else
    ERROR("Jack got command to disconnect {} but not connected.", port);
}

void jack::send_midi(const std::string &port_name, io_bytes_reader midi_data) {
  DEBUG("Sending MIDI from rtpmidi to jack port {}:", port_name);
  midi_data.print_hex();

  auto msg_buffer = ports[port_name].out_buffer;
  auto size_buffer = ports[port_name].out_buffer;
  auto midi_data_len = midi_data.size();

  while (midi_data.position < midi_data.end) {
    const char v = (char)midi_data.read_uint8();
    jack_ringbuffer_write(msg_buffer, &v, 1);
    jack_ringbuffer_write(size_buffer, (char*)&midi_data_len, sizeof(midi_data_len));
  }
}

int jack::process_callback(jack_nframes_t nframes, void *arg) {
  auto instance = *(jack *)arg;
  jack_midi_event_t ev;
  jack_time_t time;

  for (const auto& p: instance.ports) {
    auto io_port = p.second;
    auto buffer = jack_port_get_buffer(io_port.in_port, nframes);
    jack_nframes_t event_count = jack_midi_get_event_count(buffer);
    if (event_count > 0) {
      for (uint32_t e = 0; e < event_count; ++e) {
        jack_midi_event_get(&ev, buffer, e);
        DEBUG("Jack MIDI event received. port: {}, event: {}");
        // Uncomment if received MIDI events are making any sense!
/*
        auto me = instance.midi_event.find(p.first);
        if (me != instance.midi_event.end())
          me->second(&ev);
*/

        // Uncomment if we want to try sending MIDI data!
/*
        void* out_port_buffer = jack_port_get_buffer(io_port.out_port, nframes);
        jack_midi_clear_buffer(out_port_buffer);

        while (jack_ringbuffer_read_space(io_port.size_buffer) > 0)
        {
          int space{};
          jack_ringbuffer_read(io_port.size_buffer, (char*)&space, sizeof(int));
          auto midiData = jack_midi_event_reserve(out_port_buffer, 0, space);

          jack_ringbuffer_read(io_port.out_buffer, (char*)midiData, space);
        }
*/
      }
    }
  }
  return 0;
}
} // namespace rtpmidid
