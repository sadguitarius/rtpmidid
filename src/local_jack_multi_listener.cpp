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

#include "local_jack_multi_listener.hpp"
#include "jack.hpp"
#include "factory.hpp"
#include "json.hpp"
#include "local_jack_peer.hpp"
#include "mididata.hpp"
#include "midipeer.hpp"
#include "midirouter.hpp"
#include "network_rtpmidi_listener.hpp"
#include "network_rtpmidi_peer.hpp"
#include "rtpmidid/iobytes.hpp"
#include "rtpmidid/logger.hpp"
// TODO: what was this here for?
// #include <alsa/seqmid.h>
#include <memory>
#include <utility>

namespace rtpmididns {

local_jack_multi_listener_t::local_jack_multi_listener_t(
    const std::string &name_, std::shared_ptr<jack_t> jack_)
    : jack(jack_), name(name_) {

  port = jack->create_port(name);
  subscribe_connection = jack->subscribe_event[port].connect(
      [this](jack_t::port_t port, const std::string &name) {
        new_jack_connection(port, name);
      });

  midi_connection = jack->midi_event[port].connect(
      [this](jack_midi_event_t *ev) { jackseq_event(ev); });

  unsubscribe_connection = jack->unsubscribe_event[port].connect(
      [this](jack_t::port_t port) { remove_jack_connection(port); });
  // TODO unsubscribe
};
local_jack_multi_listener_t::~local_jack_multi_listener_t() {
  jack->remove_port(port);
}

midipeer_id_t
local_jack_multi_listener_t::new_jack_connection(const jack_t::port_t &port,
                                                 const std::string &name) {
  DEBUG("New connection to network peer {}, from a local connection to {}",
        name, this->name);

  midipeer_id_t networkpeer_id = MIDIPEER_ID_INVALID;
  router->for_each_peer<network_rtpmidi_listener_t>(
      [&name, &networkpeer_id](auto *peer) {
        if (peer->name_ == name) {
          peer->use_count++;
          networkpeer_id = peer->peer_id;
          DEBUG("One more user for peer: {}, count: {}", peer->peer_id,
                peer->use_count);
        }
      });

  if (networkpeer_id == MIDIPEER_ID_INVALID) {
    std::shared_ptr<midipeer_t> networkpeer =
        make_network_rtpmidi_listener(name);
    networkpeer_id = router->add_peer(networkpeer);

    jackpeers[port] = networkpeer_id;
    router->connect(networkpeer_id, peer_id);
  }

  // return std::make_pair(jackpeer_id, networkpeer_id);
  return networkpeer_id;
}

void local_jack_multi_listener_t::remove_jack_connection(
    const jack_t::port_t &port) {
  auto networkpeerI = jackpeers.find(port);
  if (networkpeerI == jackpeers.end()) {
    DEBUG("Removed Jack port {}:{}, removing midipeer. NOT FOUND!", port.client,
          port.port);
    for (auto &peers : jackpeers) {
      DEBUG("Known peer {}:{}", peers.first.port, peers.first.client);
    }
    return;
  }
  auto midipeer = router->get_peer_by_id(networkpeerI->second).get();
  network_rtpmidi_listener_t *rtppeer =
      dynamic_cast<network_rtpmidi_listener_t *>(midipeer);
  if (!rtppeer) {
    ERROR("Invalid router id {} is not a rtpmidiserverlistener!",
          networkpeerI->second);
    if (midipeer == nullptr) {
      ERROR("It is a nullptr");
    } else {
      INFO("It is a {}", midipeer->get_type());
    }
    return;
  }

  rtppeer->use_count--;

  INFO("One less user of peer: {}, use_count: {}", rtppeer->peer_id,
       rtppeer->use_count);
  if (rtppeer->use_count > 0) {
    return;
  }
  DEBUG("Removed Jack port {}:{}, removing midipeer {}", port.client, port.port,
        networkpeerI->second);
  router->remove_peer(networkpeerI->second);
}

void local_jack_multi_listener_t::jackseq_event(jack_midi_event_t *event) {
  auto peerI =
      jackpeers.find(jack_t::port_t{event->source.client, event->source.port});
  if (peerI == jackpeers.end()) {
    WARNING("Unknown source for event {}:{}!", event->source.client,
            event->source.port);
    for (auto &it : jackpeers) {
      DEBUG("Known: {}:{}", it.first.client, it.first.port);
    }
    return;
  }
  rtpmidid::io_bytes_writer_static<1024> writer;
  jacktrans_decoder.ev_to_mididata_f(event, writer,
                                     [this](const mididata_t &mididata) {
                                       router->send_midi(peer_id, mididata);
                                     });
}

void local_jack_multi_listener_t::send_midi(midipeer_id_t from,
                                            const mididata_t &data) {
  for (auto &peer : jackpeers) {
    // DEBUG("Look for dest jack peer: {} == {} ? {}", peer.second, from,
    //       peer.second == from);
    if (peer.second == from) {
      auto mididata_copy =
          mididata_t(data); // Its just the pointers, not the data itself
      auto port = peer.first;
      jacktrans_encoder.mididata_to_evs_f(
          mididata_copy, [this, port](jack_midi_event_t *ev) {
            // TODO: make this work
            /* // DEBUG("Send to Jack port {}:{}", port.client, port.port);
            snd_seq_ev_set_source(ev, this->port);
            snd_seq_ev_set_dest(ev, port.client, port.port);
            snd_seq_ev_set_direct(ev);
            auto result = snd_seq_event_output(seq->seq, ev);
            if (result < 0) {
              ERROR("Error: {}", snd_strerror(result));
              snd_seq_drop_input(seq->seq);
              snd_seq_drop_output(seq->seq);
            }
            result = snd_seq_drain_output(seq->seq);
            if (result < 0) {
              ERROR("Error: {}", snd_strerror(result));
              snd_seq_drop_input(seq->seq);
              snd_seq_drop_output(seq->seq);
            } */
          });
    }
  }
}
json_t local_jack_multi_listener_t::status() {
  json_t connections{};
  for (auto &peer : jackpeers) {
    auto port = peer.first;
    auto to = peer.second;
    connections.push_back({
        //
        {"jack", fmt::format("{}:{}", port.client, port.port)},
        {"local", to} //
    });
  }

  return json_t{
      {"name", name}, //
      {"connections", connections}
      //
  };
}

} // namespace rtpmididns
