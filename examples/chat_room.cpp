/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#include <string>
#include <iostream>
#include "unistd.h"

#include <boost/asio.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT) && defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <aedis.hpp>
#include "print.hpp"
#include "reconnect.hpp"

// Include this in no more than one .cpp file.
#include <aedis/src.hpp>

namespace net = boost::asio;
using namespace net::experimental::awaitable_operators;
using stream_descriptor = net::use_awaitable_t<>::as_default_on_t<net::posix::stream_descriptor>;
using signal_set_type = net::use_awaitable_t<>::as_default_on_t<net::signal_set>;

using aedis::adapt;
using aedis::resp3::request;
using aedis::resp3::node;

// Chat over redis pubsub. To test, run this program from different
// terminals and type messages to stdin.

// Receives Redis server-side pushes.
auto receiver(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (std::vector<node<std::string>> resp;;) {
      co_await conn->async_receive(adapt(resp));
      print_push(resp);
      resp.clear();
   }
}

// Publishes messages to other users.
auto publisher(std::shared_ptr<stream_descriptor> in, std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   for (std::string msg;;) {
      auto n = co_await net::async_read_until(*in, net::dynamic_buffer(msg, 1024), "\n");
      request req;
      req.push("PUBLISH", "chat-channel", msg);
      co_await conn->async_exec(req);
      msg.erase(0, n);
   }
}

// Sends HELLO and subscribes to channel everytime a connection is
// stablished.
auto subscriber(std::shared_ptr<connection> conn) -> net::awaitable<void>
{
   request req;
   req.get_config().cancel_on_connection_lost = true;
   req.push("HELLO", 3);
   req.push("SUBSCRIBE", "chat-channel");

   co_await conn->async_exec(req);
}

auto async_main() -> net::awaitable<void>
{
   auto ex = co_await net::this_coro::executor;
   auto conn = std::make_shared<connection>(ex);
   auto stream = std::make_shared<stream_descriptor>(ex, ::dup(STDIN_FILENO));
   signal_set_type sig{ex, SIGINT, SIGTERM};

   co_await ((run(conn) || publisher(stream, conn) || receiver(conn) ||
         healthy_checker(conn) || sig.async_wait()) && subscriber(conn));
}

auto main() -> int
{
   try {
      net::io_context ioc{1};
      co_spawn(ioc, async_main(), net::detached);
      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << "Exception: " << e.what() << std::endl;
      return 1;
   }
}

#else // defined(BOOST_ASIO_HAS_CO_AWAIT) && defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
auto main() -> int {std::cout << "Requires coroutine support." << std::endl; return 0;}
#endif // defined(BOOST_ASIO_HAS_CO_AWAIT) && defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
