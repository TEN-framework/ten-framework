/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2021 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include "shrpx_quic_listener.h"
#include "shrpx_worker.h"
#include "shrpx_config.h"
#include "shrpx_log.h"

namespace shrpx {

namespace {
void readcb(struct ev_loop *loop, ev_io *w, int revent) {
  auto l = static_cast<QUICListener *>(w->data);
  l->on_read();
}
} // namespace

QUICListener::QUICListener(const UpstreamAddr *faddr, Worker *worker)
  : faddr_{faddr}, worker_{worker} {
  ev_io_init(&rev_, readcb, faddr_->fd, EV_READ);
  rev_.data = this;
  ev_io_start(worker_->get_loop(), &rev_);
}

QUICListener::~QUICListener() {
  ev_io_stop(worker_->get_loop(), &rev_);
  close(faddr_->fd);
}

void QUICListener::on_read() {
  sockaddr_union su;
  std::array<uint8_t, 64_k> buf;
  size_t pktcnt = 0;
  iovec msg_iov{buf.data(), buf.size()};

  msghdr msg{};
  msg.msg_name = &su;
  msg.msg_iov = &msg_iov;
  msg.msg_iovlen = 1;

  uint8_t msg_ctrl[CMSG_SPACE(sizeof(int)) + CMSG_SPACE(sizeof(in6_pktinfo)) +
                   CMSG_SPACE(sizeof(int))];
  msg.msg_control = msg_ctrl;

  auto quic_conn_handler = worker_->get_quic_connection_handler();

  for (; pktcnt < 10;) {
    msg.msg_namelen = sizeof(su);
    msg.msg_controllen = sizeof(msg_ctrl);

    auto nread = recvmsg(faddr_->fd, &msg, 0);
    if (nread == -1) {
      return;
    }

    // Packets less than 21 bytes never be a valid QUIC packet.
    if (nread < 21) {
      ++pktcnt;

      continue;
    }

    if (util::quic_prohibited_port(util::get_port(&su))) {
      ++pktcnt;

      continue;
    }

    Address local_addr{};
    if (util::msghdr_get_local_addr(local_addr, &msg, su.storage.ss_family) !=
        0) {
      ++pktcnt;

      continue;
    }

    util::set_port(local_addr, faddr_->port);

    ngtcp2_pkt_info pi{
      .ecn = util::msghdr_get_ecn(&msg, su.storage.ss_family),
    };

    auto gso_size = util::msghdr_get_udp_gro(&msg);
    if (gso_size == 0) {
      gso_size = static_cast<size_t>(nread);
    }

    auto data = std::span{std::begin(buf), static_cast<size_t>(nread)};

    for (;;) {
      auto datalen = std::min(data.size(), gso_size);

      ++pktcnt;

      if (LOG_ENABLED(INFO)) {
        LOG(INFO) << "QUIC received packet: local="
                  << util::to_numeric_addr(&local_addr) << " remote="
                  << util::to_numeric_addr(&su.sa, msg.msg_namelen)
                  << " ecn=" << log::hex << pi.ecn << log::dec << " " << datalen
                  << " bytes";
      }

      // Packets less than 21 bytes never be a valid QUIC packet.
      if (datalen < 21) {
        break;
      }

      Address remote_addr;
      remote_addr.su = su;
      remote_addr.len = msg.msg_namelen;

      quic_conn_handler->handle_packet(faddr_, remote_addr, local_addr, pi,
                                       {std::begin(data), datalen});

      data = data.subspan(datalen);
      if (data.empty()) {
        break;
      }
    }
  }
}

} // namespace shrpx
