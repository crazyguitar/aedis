/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <string>
#include <iostream>
#include <charconv>

#include <aedis/aedis.hpp>

#include "types.hpp"
#include "utils.ipp"

using aedis::command;
using aedis::resp3::type;
using aedis::resp3::request;
using aedis::resp3::async_read;
using aedis::resp3::node;
using aedis::resp3::response_adapter;
using aedis::resp3::adapter_ignore;
using aedis::resp3::adapt;

namespace net = aedis::net;

request<command>
make_request()
{
  std::vector<int> vec {1, 2, 3, 4, 5, 6};

  request<command> req;
  req.push(command::hello, 3);
  req.push_range(command::rpush, "key2", std::cbegin(vec), std::cend(vec));
  req.push(command::lrange, "key2", 0, -1);
  req.push(command::lrange, "key2", 0, -1);
  req.push(command::lrange, "key2", 0, -1);
  req.push(command::lrange, "key2", 0, -1);
  req.push(command::lrange, "key2", 0, -1);
  req.push(command::lrange, "key2", 0, -1);
  req.push(command::quit);

  return req;
}

net::awaitable<void> ping()
{
   try {
      auto socket = co_await connect();
      auto req = make_request();

      co_await async_write(socket, req);

      std::string buffer;

      co_await async_read(socket, buffer); // hello
      co_await async_read(socket, buffer); // rpush

      std::vector<std::string> svec; // Response as std::vector<std::string>.
      co_await async_read(socket, buffer, adapt(svec)); // lrange

      std::list<std::string> slist; // Response as list.
      co_await async_read(socket, buffer, adapt(slist)); // lrange

      std::deque<std::string> sdeq; // Response as list.
      co_await async_read(socket, buffer, adapt(sdeq)); // lrange

      std::list<int> list; // Response as list.
      co_await async_read(socket, buffer, adapt(list)); // lrange

      std::vector<int> vec; // Response as vector.
      co_await async_read(socket, buffer, adapt(vec)); // lrange

      std::deque<int> deq; // Response as deque.
      co_await async_read(socket, buffer, adapt(deq)); // lrange

      for (auto e: svec) std::cout << e << " ";
      std::cout << std::endl;
      for (auto e: slist) std::cout << e << " ";
      std::cout << std::endl;
      for (auto e: list) std::cout << e << " ";
      std::cout << std::endl;
      for (auto e: vec) std::cout << e << " ";
      std::cout << std::endl;
      for (auto e: sdeq) std::cout << e << " ";
      std::cout << std::endl;
      for (auto e: deq) std::cout << e << " ";
      std::cout << std::endl;

      co_await async_read(socket, buffer); // quit.

   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}

int main()
{
   net::io_context ioc;
   co_spawn(ioc, ping(), net::detached);
   ioc.run();
}

/// \example custom_response2.cpp