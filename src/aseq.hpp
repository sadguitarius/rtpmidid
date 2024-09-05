/**
 * Real Time Protocol Music Instrument Digital Interface Daemon
 * Copyright (C) 2019-2023 David Moreno Montero <dmoreno@coralbits.com>
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
#include <alsa/asoundlib.h>
#include <fmt/format.h>
#include <memory>
#include <rtpmidid/exceptions.hpp>
#include <rtpmidid/iobytes.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/signal.hpp>

namespace rtpmididns {
class aseq_t : public std::enable_shared_from_this<aseq_t> {
  NON_COPYABLE_NOR_MOVABLE(aseq_t)
public:
  struct port_t {
    uint8_t client;
    uint8_t port;

    port_t() : client(0), port(0) {}
    // NOLINTNEXTLINE
    port_t(uint8_t client_, uint8_t port_) : client(client_), port(port_) {}

    bool operator<(const port_t &other) const {
      return client < other.client && port < other.port;
    }
    bool operator==(const port_t &other) const {
      return client == other.client && port == other.port;
    }

    std::string to_string() const {
      return fmt::format("port_t[{}, {}]", client, port);
    }
  };

  struct connection_t {
    std::shared_ptr<aseq_t> aseq;
    port_t from;
    port_t to;
    bool connected = false;

    // NOLINTNEXTLINE
    connection_t(const std::shared_ptr<aseq_t> &aseq_, const port_t &from_,
                 const port_t &to_)
        : aseq(aseq_), from(from_), to(to_){};
    connection_t(const connection_t &other) = delete;
    connection_t(connection_t &&other) noexcept
        : aseq(other.aseq), from(other.from), to(other.to) {
      other.from = {0, 0};
      other.to = {0, 0};
      other.connected = false;
    };
    connection_t &operator=(const connection_t &other) = delete;
    connection_t &operator=(connection_t &&other) noexcept {
      aseq = other.aseq;
      from = other.from;
      to = other.to;
      connected = other.connected;
      other.from = {0, 0};
      other.to = {0, 0};
      other.connected = false;
      return *this;
    }
    void disconnect() {
      if (connected)
        aseq->disconnect(from, to);
      connected = false;
    }
    // NOLINTNEXTLINE
    ~connection_t() { disconnect(); }
  };

  enum client_type_e {
    TYPE_HARDWARE,
    TYPE_SOFTWARE,
    TYPE_SYSTEM, // generated by ALSA, announcements and such. Not really for
                 // exports.
  };

  std::string name;
  snd_seq_t *seq;
  // std::vector<int> fds; // Normally 1?
  std::map<int, rtpmidid::signal_t<port_t, const std::string &>>
      subscribe_event;
  std::map<int, rtpmidid::signal_t<port_t>> unsubscribe_event;
  std::map<int, rtpmidid::signal_t<snd_seq_event_t *>> midi_event;
  uint8_t client_id;
  std::vector<rtpmidid::poller_t::listener_t> aseq_listener;
  rtpmidid::signal_t<const std::string &, aseq_t::client_type_e, const port_t &>
      added_port_announcement;
  rtpmidid::signal_t<const port_t &> removed_port_announcement;

  aseq_t(std::string name);
  ~aseq_t();

  void read_ready();
  std::string get_client_name(snd_seq_addr_t *addr);
  aseq_t::client_type_e get_client_type(snd_seq_addr_t *addr);

  uint8_t create_port(const std::string &name, bool do_export = true);
  void remove_port(uint8_t port);

  /// Connect two ports
  connection_t connect(const port_t &from, const port_t &to);
  void disconnect(const port_t &from, const port_t &to);
  /// Disconnects everything from this port
  void disconnect_port(uint8_t port);

  uint8_t find_device(const std::string &name);
  uint8_t find_port(uint8_t device_id, const std::string &name);
  void for_devices(
      std::function<void(uint8_t, const std::string &, client_type_e)>);
  void for_ports(uint8_t device_id,
                 std::function<void(uint8_t, const std::string &)>);
  void for_connections(const port_t &port, std::function<void(const port_t &)>);
};

std::vector<std::string> get_ports(aseq_t *);

/**
 * @short This class allows to feed midi data and loops over the given function
 *
 * As the midi data can be partial, it keeps some state to allow several calls
 * Its just a intermediary to alsa functions
 */
class mididata_to_alsaevents_t {
  NON_COPYABLE_NOR_MOVABLE(mididata_to_alsaevents_t)
public:
  snd_midi_event_t *buffer;
  mididata_to_alsaevents_t();
  ~mididata_to_alsaevents_t();

  // Gets a data bunch of bytes, and calls a callback with all found events.
  void mididata_to_evs_f(rtpmidid::io_bytes_reader &data,
                         std::function<void(snd_seq_event_t *)>);
  void ev_to_mididata(snd_seq_event_t *, rtpmidid::io_bytes_writer &data);
};

class alsa_connect_exception : public rtpmidid::exception {
public:
  alsa_connect_exception(const std::string &msg) : rtpmidid::exception(msg) {}
};

} // namespace rtpmididns

namespace std {
template <> struct hash<rtpmididns::aseq_t::port_t> {
  size_t operator()(const rtpmididns::aseq_t::port_t &key) const {
    return (key.client << 8) + key.port;
  }
};
} // namespace std

template <>
struct fmt::formatter<rtpmididns::aseq_t::port_t>
    : formatter<fmt::string_view> {
  fmt::appender format(rtpmididns::aseq_t::port_t c, format_context &ctx) const;
};

template <>
struct fmt::formatter<rtpmididns::aseq_t::client_type_e>
    : formatter<fmt::string_view> {
  fmt::appender format(rtpmididns::aseq_t::client_type_e c,
                       format_context &ctx) const;
};

template <>
struct fmt::formatter<rtpmididns::aseq_t::connection_t>
    : formatter<fmt::string_view> {
  fmt::appender format(const rtpmididns::aseq_t::connection_t &c,
                       format_context &ctx) const;
};

// template <>
// struct fmt::formatter<snd_seq_event_type>
//     : formatter<fmt::string_view> {
//   fmt::appender format(const snd_seq_event_type &c,
//                        format_context &ctx);
// };
template <> struct fmt::formatter<snd_seq_event_type> : fmt::formatter<int> {
  auto format(snd_seq_event_type type, fmt::format_context &ctx) {
    return fmt::formatter<int>::format((int)type, ctx);
  }
};
