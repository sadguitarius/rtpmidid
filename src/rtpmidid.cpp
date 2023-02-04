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
// #include <alsa/seq_event.h>
#include "./rtpmidid.hpp"
#include "./config.hpp"
#include "./stringpp.hpp"
#include "rtpmidid/exceptions.hpp"
#include <libremidi/libremidi.hpp>
#include <rtpmidid/iobytes.hpp>
#include <rtpmidid/logger.hpp>
#include <rtpmidid/rtpclient.hpp>
#include <rtpmidid/rtpserver.hpp>
#include <string>

using namespace rtpmidid;
using namespace std::chrono_literals;

rtpmidid_t::rtpmidid_t(const config_t &config)
    : server_name(config.name),
      backend(fmt::format("rtpmidi {}", server_name)) {
  setup_mdns();
  setup_midi_backend();

  for (auto &port : config.ports) {
    auto server = add_rtpmidid_import_server(config.name, port);
    servers.push_back(std::move(server));
  }

  for (auto &connect_to : config.connect_to) {
    auto res = add_rtpmidi_client(connect_to);
    if (res == std::nullopt) {
      throw rtpmidid::exception("Invalid address to connect to. Aborting.");
    }
  }
}

std::optional<std::string>
rtpmidid_t::add_rtpmidi_client(const std::string &connect_to) {
  INFO("Connecting to {}", connect_to);
  std::vector<std::string> s;
  auto find_sbracket = connect_to.find('[');
  if (find_sbracket != std::string::npos) {
    if (find_sbracket != 0)
      s.push_back(connect_to.substr(0, find_sbracket - 1));

    auto find_ebracket = connect_to.find(']');
    if (!find_ebracket) {
      ERROR("Error on address. For IPV6 Address, use name:[ipv6]:port. {}",
            connect_to);
      return std::nullopt;
    }
    s.push_back(connect_to.substr(find_sbracket + 1,
                                  find_ebracket - find_sbracket - 1));

    if (find_ebracket + 2 < connect_to.size())
      s.push_back(connect_to.substr(find_ebracket + 2, std::string::npos));
  } else {
    s = ::rtpmidid::split(connect_to, ':');
  }

  if (s.size() == 1) {
    return add_rtpmidi_client(s[0], s[0], "5004");
  } else if (s.size() == 2) {
    return add_rtpmidi_client(s[0], s[0], s[1]);
  } else if (s.size() == 3) {
    return add_rtpmidi_client(s[0], s[1], s[2]);
  } else {
    ERROR("Invalid remote address. Format is host, name:host, or "
          "name:host:port. Host can be a hostname, ip4 address, or [ip6] "
          "address (ip6:[::1]:5004). {}",
          s.size());
    return std::nullopt;
  }
}

void rtpmidid_t::announce_rtpmidid_server(const std::string &name,
                                          uint16_t port) {
  mdns_rtpmidi.announce_rtpmidi(name, port);
}

void rtpmidid_t::unannounce_rtpmidid_server(const std::string &name,
                                            uint16_t port) {
  mdns_rtpmidi.unannounce_rtpmidi(name, port);
}

std::shared_ptr<rtpserver>
rtpmidid_t::add_rtpmidid_import_server(const std::string &name,
                                       const std::string &port) {
  auto rtpserver = std::make_shared<::rtpmidid::rtpserver>(name, port);

  announce_rtpmidid_server(name, rtpserver->control_port);

  auto wrtpserver = std::weak_ptr(rtpserver);
  rtpserver->connected_event.connect(
      [this, wrtpserver,
       port](const std::shared_ptr<::rtpmidid::rtppeer> &peer) {
        if (wrtpserver.expired()) {
          return;
        }
        auto rtpserver = wrtpserver.lock();

        INFO("Remote client connects to local server at port {}. Name: {}",
             port, peer->remote_name);
        backend.create_port(peer->remote_name);

        peer->midi_event.connect([this, port](io_bytes_reader pb) {
          this->recv_rtpmidi_event(port, pb);
        });
        backend.midi_event[port].connect(
            [this, port](const libremidi::message &ev) {
              DEBUG("Got MIDI event from {}, type {:02x}", port,
                    int(ev.get_message_type()));
              auto peer_it = known_servers_connections.find(port);
              if (peer_it == std::end(known_servers_connections)) {
                WARNING("Got MIDI event in an non existing anymore peer.");
                return;
              }
              auto conn = &peer_it->second;

              io_bytes_writer_static<4096> stream;
              backend_midi_to_midiprotocol(ev, stream);
              conn->peer->send_midi(stream);
            });
        peer->disconnect_event.connect([this, port](auto reason) {
          DEBUG("Remove aseq port {}", port);
          backend.remove_port(port);
          known_servers_connections.erase(port);
        });

        server_conn_info server_conn = {
            peer->remote_name,
            peer,
            rtpserver,
        };

        known_servers_connections[port] = server_conn;
      });

  return rtpserver;
}

std::shared_ptr<rtpserver>
rtpmidid_t::add_rtpmidid_export_server(const std::string &name,
                                       const std::string &backend_port,
                                       midi_backend::port_t &from) {

  for (auto &alsa_server : alsa_to_server) {
    auto server = alsa_server.second;
    if (server->name == name) {
      INFO("Already a rtpserver for this ALSA name at {}:{} / {}. RTPMidi "
           "port: {}",
           from.client, from.port, name, server->control_port);
      return server;
    }
  }

  auto server = std::make_shared<rtpserver>(name, "");

  announce_rtpmidid_server(name, server->control_port);

  backend.midi_event[backend_port].connect(
      [server](const libremidi::message &ev) {
        DEBUG("Got MIDI event from server, type {:02x}",
              int(ev.get_message_type()));
        io_bytes_writer_static<4096> buffer;
        backend_midi_to_midiprotocol(ev, buffer);
        server->send_midi_to_all_peers(buffer);
      });

  backend.unsubscribe_event[backend_port].connect(
      [this, name, server](const midi_backend::port_t &from) {
        // This should destroy the server.
        unannounce_rtpmidid_server(name, server->control_port);
        // TODO: disconnect from on_midi_event.
        alsa_to_server.erase(from);
      });

  server->midi_event.connect([this, backend_port](io_bytes_reader buffer) {
    this->recv_rtpmidi_event(backend_port, buffer);
  });

  alsa_to_server[from] = server;

  return server;
}

void rtpmidid_t::setup_midi_backend() {
  // Export only one, but all data that is connected to it.
  // add_export_port();
  backend.create_port("Network");
  backend.subscribe_event[std::string("Network")].connect(
      [this](midi_backend::port_t from, const std::string &name) {
        DEBUG("Connected to ALSA port {}:{}. Create network server for this "
              "alsa data.",
              from.client, from.port);

        add_rtpmidid_export_server(fmt::format("{}/{}", server_name, name),
                                   "Network", from);
      });
}

void rtpmidid_t::setup_mdns() {
  mdns_rtpmidi.discover_event.connect([this](const std::string &name,
                                             const std::string &address,
                                             const std::string &port) {
    this->add_rtpmidi_client(name, address, port);
  });

  mdns_rtpmidi.remove_event.connect([this](const std::string &name) {
    // TODO : remove client / alsa sessions
    this->remove_rtpmidi_client(name);
  });
}

/** @short Adds a known client to the list of known clients.
 *
 * This does not connect yet, just adds to the list of known remote clients
 *
 * As it exists remotely,also adds local alsa ports, that when connected
 * will create the real connection.
 *
 * And when disconnected, will disconnect real connection if it's last connected
 * endpoint.
 */
std::optional<std::string>
rtpmidid_t::add_rtpmidi_client(const std::string &name,
                               const std::string &address,
                               const std::string &net_port) {
  for (auto &known : known_clients) {
    if (known.second.name == name) {
      // DEBUG(
      //     "Trying to add again rtpmidi {}:{} server. Quite probably mDNS re
      //     announce. " "Maybe somebody ask, or just periodically.", address,
      //     net_port
      // );
      known.second.addresses.push_back({address, net_port});
      return std::nullopt;
    }
  }

  backend.create_port(name);
  auto peer_info = ::rtpmidid::client_info{
      name, {{address, net_port}}, 0, 0, nullptr,
  };

  INFO("New MIDI port connects to host: {}, port: {}, name: {}", address,
       net_port, name);
  known_clients[name] = std::move(peer_info);

  backend.subscribe_event[name].connect(
      [this](const midi_backend::port_t &port, const std::string &name) {
        DEBUG("Callback on subscribe at rtpmidid: {}", name);
        connect_client(fmt::format("{}/{}", this->server_name, name), name);
      });
  backend.unsubscribe_event[name].connect(
      [this, name](const midi_backend::port_t &port) {
        auto peer_info = &known_clients[name];
        if (peer_info->use_count > 0)
          peer_info->use_count--;

        DEBUG("Callback on unsubscribe at peer {} rtpmidid (users {})",
              peer_info->name, peer_info->use_count);
        if (peer_info->use_count <= 0) {
          DEBUG("Real disconnection, no more users");
          peer_info->peer = nullptr;
        }
      });
  backend.midi_event[name].connect([this, name](const libremidi::message &ev) {
    this->recv_backend_event(name, ev);
  });

  return name;
}

void rtpmidid_t::remove_rtpmidi_client(const std::string &name) {
  INFO("Removing rtp midi client {}", name);

  for (auto &known_client : known_clients) {
    if (known_client.second.name == name) {
      auto &known = known_client;
      DEBUG("Found client to delete: alsa port {}. Deletes all known addreses.",
            known.first);
      remove_client(known.first);
      return;
    }
  }
  // WARNING("Service is not currently known to delete: {}", name);
}

void rtpmidid_t::connect_client(const std::string &name,
                                const std::string &port) {
  auto peer_info = &known_clients[port];
  if (peer_info->peer) {
    if (peer_info->peer->peer.status == rtppeer::CONNECTED) {
      peer_info->use_count++;
      DEBUG("Already connected {}. (users {})", peer_info->name,
            peer_info->use_count);
    } else {
      DEBUG("Already connecting.");
    }
  } else {
    auto &address = peer_info->addresses[peer_info->addr_idx];
    peer_info->peer = std::make_shared<rtpclient>(name);
    peer_info->peer->peer.midi_event.connect([this, port](io_bytes_reader pb) {
      this->recv_rtpmidi_event(port, pb);
    });
    peer_info->peer->peer.disconnect_event.connect(
        [this, port](rtppeer::disconnect_reason_e reason) {
          this->disconnect_client(port, reason);
        });
    peer_info->use_count++;
    DEBUG("Subscribed another local client to peer {} at rtpmidid (users {})",
          peer_info->name, peer_info->use_count);

    peer_info->peer->connect_to(address.address, address.port);
  }
}

void rtpmidid_t::disconnect_client(const std::string &port, int reasoni) {
  constexpr const char *failure_reasons[] = {"",
                                             "can't connect",
                                             "peer disconnected",
                                             "connection refused",
                                             "disconnect",
                                             "connection timeout",
                                             "CK timeout"};

  auto peer_info = &known_clients[port];
  auto reason = static_cast<rtppeer::disconnect_reason_e>(reasoni);

  DEBUG("Disconnect backend port {}, signal: {}({})", port,
        failure_reasons[reason], reason);
  // If cant connec t(network problem) or rejected, try again in next
  // address.
  switch (reason) {
  case rtppeer::disconnect_reason_e::CANT_CONNECT:
  case rtppeer::disconnect_reason_e::CONNECTION_REJECTED:
    if (peer_info->connect_attempts >= (3 * peer_info->addresses.size())) {
      ERROR("Too many attempts to connect. Not trying again. Attempted "
            "{} times.",
            peer_info->connect_attempts);
      remove_client(peer_info->name);
      return;
    }

    peer_info->connect_attempts += 1;
    peer_info->peer->connect_timer = poller.add_timer_event(1s, [peer_info] {
      peer_info->addr_idx =
          (peer_info->addr_idx + 1) % peer_info->addresses.size();
      DEBUG("Try connect next in list. Idx {}/{}", peer_info->addr_idx,
            peer_info->addresses.size());
      // Try next
      auto &address = peer_info->addresses[peer_info->addr_idx];
      peer_info->peer->connect_to(address.address, address.port);
    });
    break;

  case rtppeer::disconnect_reason_e::CONNECT_TIMEOUT:
  case rtppeer::disconnect_reason_e::CK_TIMEOUT:
    WARNING("Timeout (during {}). Keep trying.",
            reason == rtppeer::disconnect_reason_e::CK_TIMEOUT ? "handshake"
                                                               : "setup");
    // remove_client(peer_info->aseq_port);
    return;

  case rtppeer::disconnect_reason_e::PEER_DISCONNECTED:
    backend.disconnect_port(peer_info->name);
    if (peer_info->use_count > 0)
      peer_info->use_count--;
    WARNING("Peer disconnected {}. Aseq disconnect. ({} users)",
            peer_info->name, peer_info->use_count);
    // Delete it, but later as we are here because of a call inside the peer
    if (peer_info->use_count == 0) {
      poller.call_later([this, port] {
        auto peer_info = &known_clients[port];
        if (peer_info)
          peer_info->peer = nullptr;
      });
    }
    // peer_info->peer = nullptr;
    // peer_info->use_count = 0;
    // remove_client(peer_info->aseq_port);
    break;

  case rtppeer::disconnect_reason_e::DISCONNECT:
    // Do nothing, another client may connect
    break;

  default:
    ERROR("Other reason: {}", reason);
    remove_client(peer_info->name);
  }
}

void rtpmidid_t::recv_rtpmidi_event(const std::string &port,
                                    io_bytes_reader &midi_data) {
  uint8_t current_command = 0;
  libremidi::message ev;

  while (midi_data.position < midi_data.end) {
    // MIDI may reuse the last command if appropiate. For example several
    // consecutive Note On
    int maybe_next_command = midi_data.read_uint8();
    if (maybe_next_command & 0x80) {
      current_command = maybe_next_command;
    } else {
      midi_data.position--;
    }
    auto type = current_command & 0xF0;

    switch (type) {
    case 0xB0: // CC
      ev = libremidi::message::control_change(current_command & 0x0F,
                                              midi_data.read_uint8(),
                                              midi_data.read_uint8());
      break;
    case 0x90:
      snd_seq_ev_clear(&ev);
      ev = libremidi::message::note_on(current_command & 0x0F,
                                       midi_data.read_uint8(),
                                       midi_data.read_uint8());
      break;
    case 0x80:
      snd_seq_ev_clear(&ev);
      ev = libremidi::message::note_off(current_command & 0x0F,
                                        midi_data.read_uint8(),
                                        midi_data.read_uint8());
      break;
    case 0xA0:
      snd_seq_ev_clear(&ev);
      ev = libremidi::message::poly_pressure(current_command & 0x0F,
                                             midi_data.read_uint8(),
                                             midi_data.read_uint8());
      break;
    case 0xC0:
      snd_seq_ev_clear(&ev);
      ev = libremidi::message::program_change(current_command & 0x0F,
                                              midi_data.read_uint8());
      break;
    case 0xD0:
      snd_seq_ev_clear(&ev);
      ev = libremidi::message::aftertouch(current_command & 0x0F,
                                          midi_data.read_uint8());
      break;
    case 0xE0: {
      snd_seq_ev_clear(&ev);
      auto lsb = midi_data.read_uint8();
      auto msb = midi_data.read_uint8();
      auto pitch_bend = ((msb << 7) + lsb) - 8192;
      // DEBUG("Pitch bend received {}", pitch_bend);
      ev = libremidi::message::pitch_bend(current_command & 0x0F, pitch_bend);
    } break;
    case 0xF0: {
      // System messages
      switch (current_command) {
      case 0xF0: { // SysEx event
        auto start = midi_data.pos() - 1;
        auto len = 2;
        libremidi::midi_bytes sysex_message;
        sysex_message.push_back(0xF0);
        try {
          while (auto i = midi_data.read_uint8() != 0xf7) {
            sysex_message.push_back(i);
            len++;
          }
        } catch (exception &e) {
          WARNING("Malformed SysEx message in buffer has no end byte");
          break;
        }
        sysex_message.push_back(0xF7);
        // TODO: not sure how this works
        //        snd_seq_ev_set_sysex(&ev, len, &midi_data.start[start]);
        ev.clear();
        ev.bytes = sysex_message;
        //        ev.assign(len, &midi_data.start[start]);
      } break;
      case 0xF1: // MTC Quarter Frame package
        snd_seq_ev_clear(&ev);
        ev.bytes = {uint8_t(libremidi::message_type::TIME_CODE),
                    midi_data.read_uint8()};
        break;
      case 0xF3: // Song select
        ev.bytes = {uint8_t(libremidi::message_type::SONG_SELECT),
                    midi_data.read_uint8()};
        break;
      case 0xFE: // Active sense
        ev.bytes = {uint8_t(libremidi::message_type::ACTIVE_SENSING)};
        break;
      case 0xF6: // Tune request
        ev.bytes = {uint8_t(libremidi::message_type::TUNE_REQUEST)};
        break;
      case 0xF8: // Clock
        ev.bytes = {uint8_t(libremidi::message_type::TIME_CLOCK)};
        break;
        /*
              case 0xF9: // Tick
                snd_seq_ev_clear(&ev);
                snd_seq_ev_set_fixed(&ev);
                ev.type = SND_SEQ_EVENT_TICK;
                break;
        */
      case 0xFF: // Clock
        ev.bytes = {uint8_t(libremidi::message_type::SYSTEM_RESET)};
        break;
      case 0xFA: // start
        ev.bytes = {uint8_t(libremidi::message_type::START)};
        break;
      case 0xFC: // stop
        ev.bytes = {uint8_t(libremidi::message_type::STOP)};
        break;
      case 0xFB: // continue
        ev.bytes = {uint8_t(libremidi::message_type::CONTINUE)};
        break;
      default:
        break;
      }
    } break;
    default:
      WARNING("MIDI command type {:02X} not implemented yet", type);
      return;
      break;
    }
    backend.send_midi(port, &ev[0], ev.size());
    // There is one delta time byte following, if there are multiple commands in
    // one frame. We ignore this
    if (midi_data.position < midi_data.end)
      midi_data.read_uint8();
    ;
  }
}

void rtpmidid_t::recv_backend_event(const std::string &port,
                                    const libremidi::message &ev) {
  DEBUG("Callback on midi event at rtpmidid, port {}", port);
  auto peer_info = &known_clients[port];
  if (!peer_info->peer) {
    ERROR("There is no peer but I received an event! This situation should "
          "NEVER happen. File a bug. Port {}",
          port);
    return;
  }

  io_bytes_writer_static<4096> stream;
  backend_midi_to_midiprotocol(ev, stream);
  peer_info->peer->peer.send_midi(stream);
}

void rtpmidid_t::backend_midi_to_midiprotocol(const libremidi::message &ev,
                                              io_bytes_writer &stream) {
  std::string message_type{};
  switch (ev.get_message_type()) {
  case libremidi::message_type::NOTE_ON:
    stream.write_uint8(0x90 | (ev.get_channel() & 0x0F));
    stream.write_uint8(ev.bytes[0]);
    stream.write_uint8(ev.bytes[1]);
    message_type = "note on";
    break;
  case libremidi::message_type::NOTE_OFF:
    stream.write_uint8(0x80 | (ev.get_channel() & 0x0F));
    stream.write_uint8(ev.bytes[0]);
    stream.write_uint8(ev.bytes[1]);
    message_type = "note off";
    break;
  case libremidi::message_type::POLY_PRESSURE:
    stream.write_uint8(0xA0 | (ev.get_channel() & 0x0F));
    stream.write_uint8(ev.bytes[0]);
    stream.write_uint8(ev.bytes[1]);
    break;
  case libremidi::message_type::CONTROL_CHANGE:
    stream.write_uint8(0xB0 | (ev.get_channel() & 0x0F));
    stream.write_uint8(ev.bytes[0]);
    stream.write_uint8(ev.bytes[1]);
    message_type = "control change";
    break;
  case libremidi::message_type::PROGRAM_CHANGE:
    stream.write_uint8(0xC0 | (ev.get_channel()));
    stream.write_uint8(ev.bytes[0] & 0x0FF);
    message_type = "program change";
    break;
  case libremidi::message_type::AFTERTOUCH:
    stream.write_uint8(0xD0 | (ev.get_channel()));
    stream.write_uint8(ev.bytes[0] & 0x0FF);
    message_type = "aftertouch";
    break;
  case libremidi::message_type::PITCH_BEND:
    stream.write_uint8(0xE0 | (ev.get_channel() & 0x0F));
    stream.write_uint8((ev.bytes[0] + 8192) & 0x07F);
    stream.write_uint8((ev.bytes[0] + 8192) >> 7 & 0x07F);
    message_type = "pitch bend";
    break;
  case libremidi::message_type::ACTIVE_SENSING:
    stream.write_uint8(0xFE);
    message_type = "active sensing";
    break;
  case libremidi::message_type::STOP:
    stream.write_uint8(0xFC);
    message_type = "stop";
    break;
  case libremidi::message_type::TIME_CLOCK:
    stream.write_uint8(0xF8);
    message_type = "clock";
    break;
  case libremidi::message_type::START:
    stream.write_uint8(0xFA);
    message_type = "start";
    break;
  case libremidi::message_type::CONTINUE:
    stream.write_uint8(0xFB);
    message_type = "stop";
    break;
  case libremidi::message_type::TIME_CODE:
    stream.write_uint8(0xF1);
    stream.write_uint8(ev.bytes[0] & 0x0FF);
    break;
  case libremidi::message_type::SYSTEM_EXCLUSIVE:
    if (ev.size() <= stream.size()) {
      for (auto i : ev.bytes) {
        stream.write_uint8(i);
      }
    } else {
      WARNING("Sysex buffer overflow! Not sending. ({} bytes needed)",
              ev.size());
    }
    break;
  default:
    WARNING("Event type not yet implemented! Not sending. {}", message_type);
    return;
  }
}

void rtpmidid_t::remove_client(const std::string &port) {
  // We add it to the poller queue as GC, as the peer
  // might be further used at this call point.
  poller.call_later([this, port] {
    if (known_clients.find(port) == known_clients.end()) {
      DEBUG("Removing peer already removed from known peers list. Port {}",
            port);
      return;
    }
    DEBUG("Removing peer from known peers list. Port {}", port);
    backend.remove_port(port);
    backend.subscribe_event[port].disconnect_all();
    backend.unsubscribe_event[port].disconnect_all();
    backend.midi_event[port].disconnect_all();

    // Last as may be used in the shutdown of the client.
    known_clients.erase(port);
  });
}
