/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <boost/redis/connection.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/detached.hpp>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

namespace net = boost::asio;
namespace redis = boost::redis;
using endpoints = net::ip::tcp::resolver::results_type;
using redis::request;
using redis::response;
using redis::ignore_t;
using redis::config;
using redis::address;
using connection = boost::asio::use_awaitable_t<>::as_default_on_t<boost::redis::connection>;

auto redir(boost::system::error_code& ec)
   { return net::redirect_error(net::use_awaitable, ec); }

// For more info see
// - https://redis.io/docs/manual/sentinel.
// - https://redis.io/docs/reference/sentinel-clients.
auto resolve_master_address(std::vector<address> const& addresses) -> net::awaitable<address>
{
   request req;
   req.push("SENTINEL", "get-master-addr-by-name", "mymaster");
   req.push("QUIT");

   auto conn = std::make_shared<connection>(co_await net::this_coro::executor);

   response<std::optional<std::array<std::string, 2>>, ignore_t> resp;
   for (auto addr : addresses) {
      boost::system::error_code ec;
      config cfg;
      cfg.addr = addr;
      // TODO: async_run and async_exec should be lauched in
      // parallel here so we can wait for async_run completion
      // before eventually calling it again.
      conn->async_run(cfg, {}, net::consign(net::detached, conn));
      co_await conn->async_exec(req, resp, redir(ec));
      conn->cancel();
      conn->reset_stream();
      if (!ec && std::get<0>(resp))
         co_return address{std::get<0>(resp).value().value().at(0), std::get<0>(resp).value().value().at(1)};
   }

   co_return address{};
}

auto co_main(config const& cfg) -> net::awaitable<void>
{
   // A list of sentinel addresses from which only one is responsive.
   // This simulates sentinels that are down.
   std::vector<address> const addresses
   { address{"foo", "26379"}
   , address{"bar", "26379"}
   , cfg.addr
   };

   auto const ep = co_await resolve_master_address(addresses);

   std::clog
      << "Host: " << ep.host << "\n"
      << "Port: " << ep.port << "\n"
      << std::flush;
}

#endif // defined(BOOST_ASIO_HAS_CO_AWAIT)
