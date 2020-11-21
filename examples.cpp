/* Copyright (c) 2019 - 2020 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <boost/asio.hpp>

#include "aedis.hpp"

namespace net = aedis::net;
using tcp = net::ip::tcp;
using tcp_socket = net::use_awaitable_t<>::as_default_on_t<tcp::socket>;

namespace this_coro = net::this_coro;

using namespace net;
using namespace aedis;

awaitable<void> example1()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   auto cmd = ping() + quit();
   co_await async_write(socket, buffer(cmd));

   resp::buffer buffer;
   resp::response res;
   co_await resp::async_read(socket, buffer, res);

   resp::print(res.res);
}

awaitable<void> example2()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   auto cmd = multi()
            + ping()
            + incr("age")
            + exec()
	    + quit()
	    ;

   co_await async_write(socket, buffer(cmd));

   resp::buffer buffer;
   for (;;) {
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      resp::print(res.res);
   }
}

awaitable<void> example3()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   std::list<std::string> a
   {"one" ,"two", "three"};

   std::set<std::string> b
   {"a" ,"b", "c"};

   std::map<std::string, std::string> c
   { {{"Name"},      {"Marcelo"}} 
   , {{"Education"}, {"Physics"}}
   , {{"Job"},       {"Programmer"}}
   };

   std::map<int, std::string> d
   { {1, {"foo"}} 
   , {2, {"bar"}}
   , {3, {"foobar"}}
   };

   auto cmd = rpush("a", a)
            + lrange("a")
            + del("a")
            + rpush("b", b)
            + lrange("b")
            + del("b")
            + hset("c", c)
            + hincrby("c", "Age", 40)
            + hmget("c", {"Name", "Education", "Job"})
            + hvals("c")
            + hlen("c")
            + hgetall("c")
            + zadd({"d"}, d)
            + zrange("d")
	    + quit()
	    ;

   co_await async_write(socket, buffer(cmd));

   resp::buffer buffer;
   for (;;) {
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      resp::print(res.res);
   }
}

awaitable<void> example4()
{
   auto ex = co_await this_coro::executor;

   tcp::resolver resv(ex);
   auto const r = resv.resolve("127.0.0.1", "6379");

   tcp_socket socket {ex};
   co_await async_connect(socket, r);

   auto cmd = subscribe("channel");
   co_await async_write(socket, buffer(cmd));
   resp::buffer buffer;
   for (;;) {
      resp::response res;
      co_await resp::async_read(socket, buffer, res);
      resp::print(res.res);
   }
}

int main()
{
   io_context ioc {1};
   co_spawn(ioc, example1(), detached);
   co_spawn(ioc, example2(), detached);
   co_spawn(ioc, example3(), detached);
   co_spawn(ioc, example4(), detached);

   ioc.run();
}

