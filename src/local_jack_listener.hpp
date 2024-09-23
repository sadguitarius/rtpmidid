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
#include "jack.hpp"
#include "midipeer.hpp"
#include "midirouter.hpp"
#include "rtpmidid/rtpclient.hpp"
#include "rtpmidid/signal.hpp"
#include "rtpmidid/utils.hpp"

namespace rtpmididns {

/**
 * @short A local Jack port waiting for connections. When connected connects to
 * a remote rtpmidi.
 *
 * The connection is empty, but if we connect to this port, it does the
 * rtppeer creation and connect to the remote server.
 *
 * This is used both by mDNS, that creates and removes this port, and for
 * manually adding remote rtpmidi ports.
 */
class local_jack_listener_t : public midipeer_t {
  NON_COPYABLE_NOR_MOVABLE(local_jack_listener_t);

public:
  std::string local_udp_port = "0";
  std::string remote_name;
  std::string local_name;
  std::vector<rtpmidid::rtpclient_t::endpoint_t> endpoints;
  std::string hostname;
  std::string port;

  // For each Jack port connected, when arrives to 0, it disconnects
  int connection_count = 0;
  std::string jackport;
  std::shared_ptr<jack_t> jack;
  rtpmidid::connection_t<jack_t::port_t, const std::string &>
      subscribe_connection;
  rtpmidid::connection_t<jack_t::port_t> unsubscribe_connection;
  rtpmidid::connection_t<jack_midi_event_t *> jackmidi_connection;

  mididata_to_jackevents_t mididata_decoder;
  mididata_to_jackevents_t mididata_encoder;

  midipeer_id_t rtpmidiclientworker_peer_id;
  // std::shared_ptr<rtpmidid::rtpclient_t> rtpclient;
  rtpmidid::rtppeer_t::status_change_event_t status_change_event_connection;

  local_jack_listener_t(const std::string &name, const std::string &hostname,
                        const std::string &port, std::shared_ptr<jack_t> aseq,
                        const std::string &local_udp_port = "0");
  ~local_jack_listener_t() override;

  void send_midi(midipeer_id_t from, const mididata_t &) override;
  const char *get_type() const override { return "local_jack_listener_t"; }
  json_t status() override;

  void add_endpoint(const std::string &hostname, const std::string &port);
  void connect_to_remote_server(const std::string &portname);
  void disconnect_from_remote_server();

  json_t command(const std::string &cmd, const json_t &data) override;
};

} // namespace rtpmididns
