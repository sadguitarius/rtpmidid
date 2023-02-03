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

jack::jack(std::string name) {
  data.client_name = std::move(name);
  data.client =
      jack_client_open(data.client_name.c_str(), JackNoStartServer, nullptr);
  if (data.client == nullptr) {
    ERROR("Failed to open Jack client.");
    return;
  }

  jack_set_port_connect_callback(
      data.client,
      [](jack_port_id_t a, jack_port_id_t b, int c, void *arg) {
        auto data = static_cast<client_data_t *>(arg);
        std::string from, to;

        for (auto p : data->ports) {
          if (jack_port_by_id(data->client, a) == p.second.out_port) {
            from = p.second.name;
          }
          if (jack_port_by_id(data->client, b) == p.second.in_port) {
            to = p.second.name;
          }
        }

        if (c == 1) {
          INFO("New Jack connection from {} to {} on {}", from, to,
               data->client_name);
          (data->subscribe_event)[from](port_t(data->client_name, to),
                                        data->client_name);
        } else {
          INFO("Removed Jack connection from {} to {} on {}", from, to,
               data->client_name);
          (data->unsubscribe_event)[from](port_t(data->client_name, to));
        }
      },
      &data);

  jack_set_process_callback(data.client, process_callback, this);

  jack_activate(data.client);
}

jack::~jack() {
  for (const auto &i : data.ports) {
    remove_port(i.first);
  }

  if (data.client != nullptr) {
    jack_client_close(data.client);
  }
}

void jack::create_port(const std::string &name) {
  port_data_t port;
  port.name = name;
  //  port.in_port = jack_port_register(client, fmt::format("{} in",
  //  name).c_str(),
  //                                    JACK_DEFAULT_MIDI_TYPE, JackPortIsInput,
  //                                    0);
  port.in_port =
      jack_port_register(data.client, fmt::format("{} in", name).c_str(),
                         JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  port.out_port =
      jack_port_register(data.client, fmt::format("{} out", name).c_str(),
                         JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  if (port.in_port == nullptr || port.out_port == nullptr) {
    ERROR("Failed to create Jack ports for {}", name);
  }

  port.size_buffer = jack_ringbuffer_create(client_data_t::ringbuffer_size);
  port.in_buffer = jack_ringbuffer_create(client_data_t::ringbuffer_size);

  data.ports[name] = std::move(port);
}

void jack::remove_port(const std::string &name) {
  if (data.ports.find(name) == data.ports.end()) {
    ERROR("Jack port {} does not exist, so cannot remove.");
    return;
  }
  jack_port_unregister(data.client, data.ports[name].in_port);
  jack_port_unregister(data.client, data.ports[name].out_port);
  jack_ringbuffer_free(data.ports[name].in_buffer);
  jack_ringbuffer_free(data.ports[name].size_buffer);
  data.ports.erase(name);
}

void jack::disconnect_port(std::string &port) {
  DEBUG("Disconnect Jack port {}", port);
  auto jack_port = jack_port_by_name(data.client, port.c_str());
  if (jack_port_get_connections(jack_port))
    jack_port_disconnect(data.client, jack_port);
  else
    ERROR("Jack got command to disconnect {} but not connected.", port);
}

void jack::midi_to_jack(const std::string &port_name,
                        io_bytes_reader &midi_data) {
  DEBUG("MIDI to Jack: received {} bytes of MIDI from {}", midi_data.size(),
        port_name);

  auto port = data.ports[port_name];
  auto in_buffer = port.in_buffer;
  //  auto size_buffer = port.size_buffer;
  //  auto midi_data_len = midi_data.size();

  midi_data.position = midi_data.start;
  while (midi_data.position < midi_data.end) {
    const char v = (char)midi_data.read_uint8();
    DEBUG("Writing MIDI byte {:#02x} to Jack input buffer", uint8_t(v));
    jack_ringbuffer_write(in_buffer, &v, 1);
    //    jack_ringbuffer_write(size_buffer, (char *)&midi_data_len,
    //                          sizeof(midi_data_len));
  }

  DEBUG("Jack received MIDI: {}");
}

int jack::process_callback(jack_nframes_t nframes, void *arg) {
  auto instance = static_cast<jack *>(arg);
  //  jack_midi_event_t ev;
  //  jack_time_t time;

  for (const auto &p : instance->data.ports) {
    //    void *in_port_buffer = jack_port_get_buffer(p.second.in_port,
    //    nframes); jack_midi_clear_buffer(in_port_buffer);
    while (jack_ringbuffer_read_space(p.second.in_buffer) > 0) {
      //      DEBUG("Jack: Writing MIDI data to {}", p.second.name);
//      int space{};
//      jack_ringbuffer_read(p.second.in_buffer, (char *)&space, sizeof(char));
      auto midiData = jack_midi_event_reserve(p.second.in_buffer, 0, 1);
      jack_ringbuffer_read(p.second.in_buffer, (char *)midiData, 1);
//      jack_ringbuffer_read_advance(p.second.in_buffer, 1);
//      DEBUG("Processing ringbuffer data {:02x}", space);

    }
  }
  /*
   */

  // process MIDI input from rtpmidi
  /*
    for (const auto &p : instance->ports) {
      auto io_port = p.second;
      auto buffer = jack_port_get_buffer(io_port.in_port, nframes);
      jack_nframes_t event_count = jack_midi_get_event_count(buffer);
      if (event_count > 0) {
        for (uint32_t e = 0; e < event_count; e++) {
          DEBUG("Jack MIDI event received. port: {}",
                jack_port_short_name(instance->ports[p.first].in_port));
          jack_midi_event_get(&ev, buffer, e);
          // Uncomment if received MIDI events are making any sense!
          // auto me = instance->midi_event.find(p.first);
          // if (me != instance->midi_event.end())
          // me->second(&ev);
        }
      }
    }
  */

  return 0;
}

signal_t<jack::port_t, const std::string &> &
jack::get_subscribe_event(const std::string &index) {
  return data.subscribe_event[index];
}

signal_t<jack::port_t> &jack::get_unsubscribe_event(const std::string &index) {
  return data.unsubscribe_event[index];
}

signal_t<jack_midi_event_t *> &jack::get_midi_event(const std::string &index) {
  return data.midi_event[index];
}
} // namespace rtpmidid
