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

#include "./midi_backend.hpp"
#include <libremidi/libremidi.hpp>
#include <memory>
#include <optional>
#include <rtpmidid/mdns_rtpmidi.hpp>
#include <rtpmidid/poller.hpp>
#include <set>
#include <string>

namespace rtpmidid {
struct config_t;
class rtpserver;
class rtpclient;
class rtppeer;
class io_bytes_reader;
class io_bytes_writer;
struct address_t {
  std::string address;
  std::string port;
};

struct client_info {
  std::string name;
  std::vector<address_t> addresses;
  int addr_idx; // Current try address, if any.
  uint16_t use_count;
  // This might be not intialized if not really connected yet.
  std::shared_ptr<::rtpmidid::rtpclient> peer;
  uint connect_attempts = 0;
};
struct server_conn_info {
  std::string name;
  // This might be not intialized if not really connected yet.
  std::shared_ptr<::rtpmidid::rtppeer> peer;
  std::shared_ptr<::rtpmidid::rtpserver> server;
};

class rtpmidid_t {
public:
  std::string server_name;
  ::rtpmidid::midi_backend backend;
  ::rtpmidid::mdns_rtpmidi mdns_rtpmidi;
  // Local port id to client_info for connections
  std::map<std::string, client_info> known_clients;
  std::map<std::string, server_conn_info> known_servers_connections;
  std::vector<std::shared_ptr<::rtpmidid::rtpserver>> servers;
  std::map<midi_backend::port_t, std::shared_ptr<::rtpmidid::rtpserver>>
      alsa_to_server;
  std::set<std::string> known_mdns_peers;

  rtpmidid_t(const config_t &config);

  // Manual connect to a server.
  std::optional<std::string>
  add_rtpmidi_client(const std::string &hostdescription);
  std::optional<std::string> add_rtpmidi_client(const std::string &name,
                                                const std::string &address,
                                                const std::string &port);
  void remove_rtpmidi_client(const std::string &name);

  void recv_rtpmidi_event(const std::string &port, io_bytes_reader &midi_data);
  void recv_backend_event(const std::string &port,
                          const libremidi::message &ev);

  static void backend_midi_to_midiprotocol(const libremidi::message &ev,
                                           io_bytes_writer &buffer);

  void setup_midi_backend();
  void setup_mdns();
  void announce_rtpmidid_server(const std::string &name, uint16_t port);
  void unannounce_rtpmidid_server(const std::string &name, uint16_t port);
  void connect_client(const std::string &name, const std::string &port);
  void disconnect_client(const std::string &port, int reason);
  // An import server is one that for each discovered connection, creates
  // the alsa ports
  std::shared_ptr<rtpserver>
  add_rtpmidid_import_server(const std::string &name, const std::string &port);

  // An export server is one that exports a local ALSA seq port. It is announced
  // with the aseq port name and so on. There is one per connection to the
  // "Network"
  std::shared_ptr<rtpserver>
  add_rtpmidid_export_server(const std::string &name,
                             const std::string &backend_port,
                             midi_backend::port_t &from);

  void remove_client(const std::string &alsa_port);
};
} // namespace rtpmidid
