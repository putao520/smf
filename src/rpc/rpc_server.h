// Copyright (c) 2016 Alexander Gallego. All rights reserved.
//
#pragma once
// std
#include <algorithm>
#include <type_traits>
// seastar
#include <core/distributed.hh>
// smf
#include "histogram/histogram.h"
#include "rpc/filters/zstd_filter.h"
#include "rpc/rpc_connection_limits.h"
#include "rpc/rpc_filter.h"
#include "rpc/rpc_handle_router.h"
#include "rpc/rpc_server_connection.h"
#include "platform/macros.h"

namespace smf {

enum RPCFLAGS : uint32_t {
  RPCFLAGS_NONE = 0,
  // TODO(agallego) - drop load
  RPCFLAGS_LOAD_SHEDDING_ON = 1
};

class rpc_server {
 public:
  rpc_server(distributed<rpc_server_stats> *stats,
             uint16_t                       port,
             uint32_t                       flags);
  ~rpc_server();

  void     start();
  future<> stop();

  future<smf::histogram> copy_histogram() {
    smf::histogram h(hist_->get());
    return make_ready_future<smf::histogram>(std::move(h));
  }

  template <typename T> void register_service() {
    static_assert(std::is_base_of<rpc_service, T>::value,
                  "register_service can only be called with a derived class of "
                  "smf::rpc_service");
    routes_.register_service(std::make_unique<T>());
  }
  template <typename Function> void register_incoming_filter(Function fn) {
    in_filters_.push_back(fn);
  }

  template <typename Function> void register_outgoing_filter(Function fn) {
    out_filters_.push_back(fn);
  }

  SMF_DISALLOW_COPY_AND_ASSIGN(rpc_server);

 private:
  future<> handle_client_connection(lw_shared_ptr<rpc_server_connection> conn);
  future<> dispatch_rpc(lw_shared_ptr<rpc_server_connection> conn,
                        rpc_recv_context &&                  ctx);

 private:
  lw_shared_ptr<server_socket>   listener_;
  distributed<rpc_server_stats> *stats_;
  const uint16_t                 port_;
  rpc_handle_router              routes_;

  using in_filter_t = std::function<future<rpc_recv_context>(rpc_recv_context)>;
  std::vector<in_filter_t> in_filters_;
  using out_filter_t = std::function<future<rpc_envelope>(rpc_envelope)>;
  std::vector<out_filter_t> out_filters_;

  std::unique_ptr<histogram> hist_ = std::make_unique<histogram>();
  uint32_t                   flags_;

  std::unique_ptr<rpc_connection_limits> limits_ =
    std::make_unique<rpc_connection_limits>();
};

}  // namespace smf
