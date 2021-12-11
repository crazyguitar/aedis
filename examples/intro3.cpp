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
using aedis::resp3::request;
using aedis::resp3::async_read;
using aedis::resp3::node;
using aedis::resp3::response_adapter;

namespace net = aedis::net;

/* A slightly more elaborate way dealing with requests and responses.
  
   This time we send the ping + quit only after the response to the
   hello command has been received.  We also separate the application
   logic out the coroutine for clarity.
 */

// Adds a new element in the queue if necessary.
void prepare_next(std::queue<request<command>>& reqs)
{
   if (std::empty(reqs) || std::size(reqs) == 1)
      reqs.push({});
}

/* The function that processes the response has been factored out of
   the coroutine to simplify application logic.
 */
void process_response(std::queue<request<command>>& reqs, std::vector<node> const& resp)
{
   std::cout
      << reqs.front().commands.front() << ":\n"
      << resp << std::endl;

   switch (reqs.front().commands.front()) {
      case command::hello:
         prepare_next(reqs);
         reqs.back().push(command::ping);
         reqs.back().push(command::quit);
         break;
      default: {};
   }
}

net::awaitable<void> ping()
{
   try {
      std::queue<request<command>> reqs;
      reqs.push({});
      reqs.back().push(command::hello, 3);

      auto socket = co_await connect();
      std::string buffer;

      while (!std::empty(reqs)) {
	 co_await async_write(socket, reqs.front());
	 while (!std::empty(reqs.front().commands)) {
            std::vector<node> resp;
            auto adapter = response_adapter(&resp);

	    co_await async_read(socket, buffer, adapter);
	    process_response(reqs, resp);
	    reqs.front().commands.pop();
	 }

	 reqs.pop();
      }

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

/// \example intro3.cpp