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
#include <fmt/format.h>
#include <rtpmidid/exceptions.hpp>
#include <rtpmidid/logger.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/rtpclient.hpp>
#include <stdio.h>
#include "./jack.hpp"

namespace rtpmidid {

jack::jack(std::string _name) : name(std::move(_name)) {
}

jack::~jack() {
}

uint8_t jack::create_port(const std::string &name) {
}

void jack::remove_port(uint8_t port) {
}

//std::vector<std::string> get_ports(aseq *seq) {
//}

//std::string jack::get_client_name(snd_seq_addr_t *addr) {
//}

void jack::disconnect_port(uint8_t port) {
}
} // namespace rtpmidid
