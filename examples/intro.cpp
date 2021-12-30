/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <aedis/aedis.hpp>

#include "utils.ipp"

using aedis::command;
using aedis::resp3::serializer;
using aedis::resp3::async_read;
using aedis::resp3::adapt;

namespace net = aedis::net;
using net::async_write;
using net::buffer;

/* Illustrates the basic principles.
 
   Sends some commands to the redis server and read the responses
   back. The hello command is always required after the connection has
   been stablished.
  
   Note: In general connections will be kept open and used for
   multiple requests, see for example echo_server.cpp.
*/

net::awaitable<void> ping()
{
   try {
      auto socket = co_await connect();

      // Creates and sends the request.
      serializer<command> sr;
      sr.push(command::hello, 3);
      sr.push(command::flushall);
      sr.push(command::ping);
      sr.push(command::incr, "key");
      sr.push(command::quit);
      co_await async_write(socket, buffer(sr.request()));

      // Expected responses.
      int incr;
      std::string ping;

      // Reads the responses.
      std::string buffer;
      std::size_t n = 0;
      n += co_await async_read(socket, buffer); // hello (ignored)
      n += co_await async_read(socket, buffer); // flushall
      n += co_await async_read(socket, buffer, adapt(ping));
      n += co_await async_read(socket, buffer, adapt(incr));
      n += co_await async_read(socket, buffer);

      // Print the responses.
      std::cout
	 << "Bytes read: " << n << "\n"
	 << "ping: " << ping << "\n"
	 << "incr: " << incr << "\n";

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

/// \example intro.cpp