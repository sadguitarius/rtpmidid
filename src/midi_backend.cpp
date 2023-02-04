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
#include "./midi_backend.hpp"
// #include <alsa/seq.h>
#include <fmt/format.h>
#include <libremidi/libremidi.hpp>
#include <rtpmidid/exceptions.hpp>
#include <rtpmidid/logger.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/rtpclient.hpp>
#include <stdio.h>

namespace rtpmidid {
void error_handler(const char *file, int line, const char *function, int err,
                   const char *fmt, ...) {
  va_list arg;
  std::string msg;
  char buffer[1024];

  if (err == ENOENT) /* Ignore those misleading "warnings" */
    return;
  va_start(arg, fmt);
  vsprintf(buffer, fmt, arg);
  msg += buffer;
  if (err) {
    msg += ": ";
    msg += snd_strerror(err);
  }
  va_end(arg);
  std::string filename = "alsa/";
  filename += file;

  logger::__logger.log(filename.c_str(), line, ::logger::LogLevel::ERROR, msg);
}

midi_backend::midi_backend(std::string name) : client_name(std::move(name)) {
  client = jack_client_open(client_name.c_str(), JackNoStartServer, nullptr);
}

midi_backend::~midi_backend() { jack_client_close(client); }

/**
 * @short data is ready at the sequencer to read
 *
 * FUTURE OPTIMIZATION: Instead of sending events one by one, send them in
 *                      groups that go to the same port. This will save some
 *                      bandwidth.
 */
void midi_backend::read_ready(const libremidi::message &ev,
                              const std::string &from) {
  // DEBUG("ALSA MIDI event: {}, pending: {} / {}", ev->type, pending,
  // snd_seq_event_input_pending(seq, 0));

  switch (ev.get_message_type()) {
    // TODO: need to handle this somewhere
    /*
        case SND_SEQ_EVENT_PORT_SUBSCRIBED: {
          // auto client = std::make_shared<rtpmidid::rtpclient>(name);
          uint8_t client, port;
          std::string name;
          snd_seq_addr_t *addr;
          if (ev->data.connect.sender.client != client_id) {
            addr = &ev->data.connect.sender;
          } else {
            addr = &ev->data.connect.dest;
          }

          name = get_client_name(addr);
          client = addr->client;
          port = addr->port;
          auto myport = ev->dest.port;
          INFO("New ALSA connection from port {} ({}:{})", name, client, port);

          subscribe_event[myport](port_t(client, port), name);
        } break;
        case SND_SEQ_EVENT_PORT_UNSUBSCRIBED: {
          auto addr = &ev->data.addr;
          auto myport = ev->dest.port;
          unsubscribe_event[myport](port_t(addr->client, addr->port));
          DEBUG("Disconnected");
        } break;
          */
  case libremidi::message_type::TIME_CLOCK:
  case libremidi::message_type::START:
  case libremidi::message_type::CONTINUE:
  case libremidi::message_type::STOP:
  case libremidi::message_type::NOTE_OFF:
  case libremidi::message_type::NOTE_ON:
  case libremidi::message_type::POLY_PRESSURE:
  case libremidi::message_type::CONTROL_CHANGE:
  case libremidi::message_type::PROGRAM_CHANGE:
  case libremidi::message_type::AFTERTOUCH:
  case libremidi::message_type::PITCH_BEND:
  case libremidi::message_type::SYSTEM_EXCLUSIVE:
  case libremidi::message_type::TIME_CODE:
  case libremidi::message_type::ACTIVE_SENSING: {
    // TODO: need destination port
    //      auto myport = ev->dest.port;
    auto me = midi_event.find(from);
    if (me != midi_event.end())
      me->second(ev);
  } break;
  default:
    static bool warning_raised[SND_SEQ_EVENT_NONE + 1];
    if (!warning_raised[int(ev.get_message_type())]) {
      warning_raised[int(ev.get_message_type())] = true;
      WARNING("This event type {} is not managed yet",
              int(ev.get_message_type()));
    }
    break;
  }
}

void midi_backend::create_port(const std::string &name) {
  ports[name] = std::make_unique<io_port_t>(client, client_name);
  // TODO set callback
  ports[name]->in_port->set_callback(
      [this, name](const libremidi::message &message) {
        read_ready(message, name);
      });
  ports[name]->in_port->open_virtual_port(fmt::format("{} in", name));
  ports[name]->out_port->open_virtual_port(fmt::format("{} out", name));
}

void midi_backend::remove_port(const std::string &name) {
  ports[name]->in_port->close_port();
  ports[name]->out_port->close_port();
  midi_event.erase(name);
}

void midi_backend::subscribe_port(const std::string &from,
                                  const std::string &to) {
  auto subs = ports[from]->subscribers;
  if (std::find(subs.begin(), subs.end(), to) == subs.end()) {
    subs.push_back(to);
  }
}

void midi_backend::unsubscribe_port(const std::string &from,
                                    const std::string &to) {
  auto subs = ports[from]->subscribers;
  subs.erase(std::remove(subs.begin(), subs.end(), to), subs.end());
}

void midi_backend::disconnect_port(const std::string &port) {
  DEBUG("Disconnect MIDI port {}", port);
  ports[port]->subscribers.clear();
  for (const auto &i : ports) {
    unsubscribe_port(i.first, port);
  }
}

void midi_backend::send_midi(const std::string &port, const uint8_t *message,
                             size_t size) {
  ports[port]->out_port->send_message(message, size);
};

} // namespace rtpmidid
