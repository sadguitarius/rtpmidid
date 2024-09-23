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
#include <cstdarg>
#include <fmt/format.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>
#include <rtpmidid/logger.hpp>
#include <rtpmidid/poller.hpp>

namespace rtpmididns {
void error_handler(const char *file, int line, const char *function, int err,
                   const char *fmt, ...) {
  // NOLINTNEXTLINE
  va_list arg;
  std::string msg;
  // NOLINTNEXTLINE
  char buffer[1024];

  if (err == ENOENT) /* Ignore those misleading "warnings" */
    return;
  // NOLINTNEXTLINE
  va_start(arg, fmt);
  // NOLINTNEXTLINE
  vsprintf(buffer, fmt, arg);
  // NOLINTNEXTLINE
  msg += buffer;
  if (err) {
    msg += ": ";
    // TODO: need jack error function
    // msg += snd_strerror(err);
  }
  // NOLINTNEXTLINE
  va_end(arg);
  std::string filename = "jack/";
  filename += file;

  logger::__logger.log(filename.c_str(), line, ::logger::LogLevel::ERROR,
                       msg.c_str());
}

jack_t::jack_t(std::string name) : name(std::move(name)) {
  // TODO: set Jack error handler
  // snd_lib_error_set_handler(error_handler);
  client = jack_client_open(name.c_str(), JackNoStartServer, nullptr);
  if (client == nullptr) {
    ERROR("Failed to open Jack client.");
    return;
  }

  /*
  jack_set_port_connect_callback(
      client,
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
  */

  // TODO: how to set callback?
  // jack_set_process_callback(data.client, process_callback, this);

  jack_activate(client);
}

jack_t::~jack_t() {
  for (const auto &i : ports) {
    remove_port(i.first);
  }

  if (client != nullptr) {
    jack_client_close(client);
  }
}

uint8_t jack_t::create_port(const std::string &name, bool do_export) {
  jack_t::port_data_t port;
  port.name = name;
  //  port.in_port = jack_port_register(client, fmt::format("{} in",
  //  name).c_str(),
  //                                    JACK_DEFAULT_MIDI_TYPE, JackPortIsInput,
  //                                    0);
  port.in_port = jack_port_register(client, fmt::format("{} in", name).c_str(),
                                    JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
  port.out_port =
      jack_port_register(client, fmt::format("{} out", name).c_str(),
                         JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
  if (port.in_port == nullptr || port.out_port == nullptr) {
    ERROR("Failed to create Jack ports for {}", name);
  }

  port.size_buffer = jack_ringbuffer_create(ringbuffer_size);
  port.in_buffer = jack_ringbuffer_create(ringbuffer_size);

  ports[name] = std::move(port);

  // TODO: what does this return?
  return 0;
}

void jack_t::remove_port(const std::string &name) {
  if (ports.find(name) == ports.end()) {
    ERROR("Jack port {} does not exist, so cannot remove.");
    return;
  }
  jack_port_unregister(client, ports[name].in_port);
  jack_port_unregister(client, ports[name].out_port);
  jack_ringbuffer_free(ports[name].in_buffer);
  jack_ringbuffer_free(ports[name].size_buffer);
  ports.erase(name);
}

void jack_t::disconnect_port(const std::string &port) {
  DEBUG("Disconnect Jack port {}", port);
  auto jack_port = jack_port_by_name(client, port.c_str());
  if (jack_port_get_connections(jack_port))
    jack_port_disconnect(client, jack_port);
  else
    ERROR("Jack got command to disconnect {} but not connected.", port);
}

/*
void jack_t::midi_to_jack(const std::string &port_name,
                          rtpmidid::io_bytes_reader &midi_data) {
  DEBUG("MIDI to Jack: received {} bytes of MIDI from {}", midi_data.size(),
        port_name);

  auto port = ports[port_name];
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
*/

/*
int jack_t::process_callback(jack_nframes_t nframes, void *arg) {
  auto instance = static_cast<jack_t *>(arg);
  //  jack_midi_event_t ev;
  //  jack_time_t time;

  for (const auto &p : instance->ports) {
    //    void *in_port_buffer = jack_port_get_buffer(p.second.in_port,
    //    nframes); jack_midi_clear_buffer(in_port_buffer);
    while (jack_ringbuffer_read_space(p.second.in_buffer) > 0) {
      //      DEBUG("Jack: Writing MIDI data to {}", p.second.name);
      //      int space{};
      //      jack_ringbuffer_read(p.second.in_buffer, (char *)&space,
      //      sizeof(char));
      auto midiData = jack_midi_event_reserve(p.second.in_buffer, 0, 1);
      jack_ringbuffer_read(p.second.in_buffer, (char *)midiData, 1);
      //      jack_ringbuffer_read_advance(p.second.in_buffer, 1);
      //      DEBUG("Processing ringbuffer data {:02x}", space);
    }
  }

  // process MIDI input from rtpmidi
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

  return 0;
}
*/
//
// TODO: these buffer sizes should probably be configurable or pinned to pool
// size in bytes
mididata_to_jackevents_t::mididata_to_jackevents_t()
    : buffer(nullptr), decode_buffer_data(65536, 0),
      decode_buffer(&decode_buffer_data[0], 65536) {
  // snd_midi_event_new(65536, &buffer);
}
mididata_to_jackevents_t::~mididata_to_jackevents_t() {
  // if (buffer)
    // snd_midi_event_free(buffer);
}

void mididata_to_jackevents_t::mididata_to_evs_f(
    rtpmidid::io_bytes_reader &data,
    std::function<void(jack_midi_event_t *)> func) {
  jack_midi_event_t ev;

  // snd_midi_event_reset_encode(buffer);

  while (data.position < data.end) {
    // DEBUG("mididata to snd_ev, left {}", data);
    // auto used = snd_midi_event_encode(buffer, data.position,
    //                                   data.end - data.position, &ev);
    // if (used <= 0) {
    //   ERROR("Fail encode event: {}, {}", used, data);
    //   data.print_hex(false);
    //   return;
    // }
    // data.position += used;
    func(&ev);
  }
}

void mididata_to_jackevents_t::ev_to_mididata_f(
    jack_midi_event_t *ev, rtpmidid::io_bytes_writer &data,
    std::function<void(const mididata_t &)> func) {
  /* if (ev->type != SND_SEQ_EVENT_SYSEX) {
    snd_midi_event_reset_decode(buffer);
    auto ret = snd_midi_event_decode(buffer, data.position,
                                     data.end - data.position, ev);
    if (ret < 0) {
      ERROR("Could not translate alsa seq event. Do nothing.");
      return;
    }

    data.position += ret;
    const auto mididata = mididata_t(data);
    func(mididata);
  } else {
    snd_midi_event_reset_decode(buffer);
    auto total_bytes = snd_midi_event_decode(buffer, &decode_buffer_data[0],
                                             ev->data.ext.len, ev);
    if (total_bytes < 0) {
      ERROR("Could not translate alsa seq event. Do nothing.");
      return;
    }
    bool start = true;

    while (true) {
      rtpmidid::io_bytes_writer_static<258> out_buffer;
      if (start) {
        decode_buffer.position += 1;
        out_buffer.write_uint8(0xF0);
        start = false;
      } else {
        out_buffer.write_uint8(0xF7);
      }
      auto bytes_left =
          total_bytes - (decode_buffer.position - decode_buffer.start) - 1;
      if (bytes_left <= 256) {
        out_buffer.copy_from(decode_buffer, bytes_left);
        out_buffer.write_uint8(0xF7);
        const auto mididata = mididata_t(out_buffer);
        func(mididata);
        decode_buffer.position = decode_buffer.start;
        return;
      } else {
        out_buffer.copy_from(decode_buffer, 256); // Don't copy 0xF7
        out_buffer.write_uint8(0xF0);
        const auto mididata = mididata_t(out_buffer);
        func(mididata);
        decode_buffer.position += 256;
      }
    }
  } */
}

} // namespace rtpmididns

fmt::appender
fmt::formatter<rtpmididns::jack_t::port_t>::format(rtpmididns::jack_t::port_t c,
                                                   format_context &ctx) const {
  auto name = fmt::format("port_t[{}, {}]", c.client, c.port);
  return formatter<fmt::string_view>::format(name, ctx);
}

fmt::appender fmt::formatter<rtpmididns::jack_t::client_type_e>::format(
    rtpmididns::jack_t::client_type_e c, format_context &ctx) const {
  auto name = c == rtpmididns::jack_t::client_type_e::TYPE_HARDWARE
                  ? "TYPE_HARDWARE"
                  : "TYPE_SOFTWARE";
  return formatter<fmt::string_view>::format(name, ctx);
}

fmt::appender fmt::formatter<rtpmididns::jack_t::connection_t>::format(
    const rtpmididns::jack_t::connection_t &c, format_context &ctx) const {
  auto name =
      fmt::format("connection_t[{}, {} -> {}]", c.connected, c.from, c.to);
  return formatter<fmt::string_view>::format(name, ctx);
}

// TODO: is there a list of Jack MIDI event types?
/*
const char *format_as(const snd_seq_event_type type) {
  switch (type) {
#define CASE(x)                                                                \
  case x:                                                                      \
    return #x
    CASE(SND_SEQ_EVENT_SYSTEM);
    CASE(SND_SEQ_EVENT_RESULT);
    CASE(SND_SEQ_EVENT_NOTE);
    CASE(SND_SEQ_EVENT_NOTEON);
    CASE(SND_SEQ_EVENT_NOTEOFF);
    CASE(SND_SEQ_EVENT_KEYPRESS);
    CASE(SND_SEQ_EVENT_CONTROLLER);
    CASE(SND_SEQ_EVENT_PGMCHANGE);
    CASE(SND_SEQ_EVENT_CHANPRESS);
    CASE(SND_SEQ_EVENT_PITCHBEND);
    CASE(SND_SEQ_EVENT_CONTROL14);
    CASE(SND_SEQ_EVENT_NONREGPARAM);
    CASE(SND_SEQ_EVENT_REGPARAM);
    CASE(SND_SEQ_EVENT_SONGPOS);
    CASE(SND_SEQ_EVENT_SONGSEL);
    CASE(SND_SEQ_EVENT_QFRAME);
    CASE(SND_SEQ_EVENT_TIMESIGN);
    CASE(SND_SEQ_EVENT_KEYSIGN);
    CASE(SND_SEQ_EVENT_START);
    CASE(SND_SEQ_EVENT_CONTINUE);
    CASE(SND_SEQ_EVENT_STOP);
    CASE(SND_SEQ_EVENT_CLOCK);
    CASE(SND_SEQ_EVENT_SENSING);
    CASE(SND_SEQ_EVENT_PORT_SUBSCRIBED);
    CASE(SND_SEQ_EVENT_PORT_UNSUBSCRIBED);
    CASE(SND_SEQ_EVENT_USR0);
    CASE(SND_SEQ_EVENT_USR1);
    CASE(SND_SEQ_EVENT_USR2);
    CASE(SND_SEQ_EVENT_USR3);
    CASE(SND_SEQ_EVENT_USR4);
    CASE(SND_SEQ_EVENT_USR5);
    CASE(SND_SEQ_EVENT_USR6);
    CASE(SND_SEQ_EVENT_USR7);
    CASE(SND_SEQ_EVENT_USR8);
    CASE(SND_SEQ_EVENT_USR9);
    CASE(SND_SEQ_EVENT_SYSEX);
    CASE(SND_SEQ_EVENT_BOUNCE);
    CASE(SND_SEQ_EVENT_USR_VAR0);
    CASE(SND_SEQ_EVENT_USR_VAR1);
    CASE(SND_SEQ_EVENT_USR_VAR2);
    CASE(SND_SEQ_EVENT_USR_VAR3);
    CASE(SND_SEQ_EVENT_USR_VAR4);
    CASE(SND_SEQ_EVENT_NONE);
#undef CASE
  default:
    return "Unknown";
  }
}
*/
