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
#include "rtpmidid/rtppeer.hpp"
#include "rtpmidid/utils.hpp"
#include <unordered_map>

namespace rtpmididns {

/**
 * @short Just the exported Network entry point (`Jack / Network`)
 *
 * This is the Jack `Network` port, which has these functionalities:
 *
 * * New Jack connections create a rtpmidid server port:
 *   * Data coming from that Jack port goes to this rtpmidid server
 *   * Data from this rtpmidid server goes to this Jack port
 *
 * The way to do it using the midirouter is when we get a new Jack
 * midi connection, we create the rtpmidid server (rtpmididpeer_t)
 * and connect them.
 *
 * When the Jack port receives Jack sequencer data, we check the
 * origin port and use that port to match to the Jack connection
 * and send the data as if it comes from there to the midirouter.
 */
class local_jack_multi_listener_t : public midipeer_t {
  NON_COPYABLE_NOR_MOVABLE(local_jack_multi_listener_t);

public:
  std::shared_ptr<jack_t> jack;
  std::string port;
  mididata_to_jackevents_t jacktrans_decoder;
  mididata_to_jackevents_t jacktrans_encoder;
  std::string name;

  std::unordered_map<jack_t::port_t, midipeer_id_t> jackpeers;
  rtpmidid::connection_t<jack_t::port_t, const std::string &>
      subscribe_connection;
  rtpmidid::connection_t<jack_t::port_t> unsubscribe_connection;
  rtpmidid::connection_t<jack_midi_event_t *> midi_connection;

  local_jack_multi_listener_t(const std::string &name,
                              std::shared_ptr<jack_t> jack);
  ~local_jack_multi_listener_t() override;
  const char *get_type() const override {
    return "local_jack_multi_listener_t";
  }

  void send_midi(midipeer_id_t from, const mididata_t &) override;
  json_t status() override;

  // Returns the RTPSERVER id. Useful for testing.
  midipeer_id_t new_jack_connection(const jack_t::port_t &port,
                                    const std::string &name);
  void remove_jack_connection(const jack_t::port_t &port);

  // received data from the jack side, look who is the jackpeer_t
  // and send pretending its it.
  void jackseq_event(jack_midi_event_t *event);
};

} // namespace rtpmididns
