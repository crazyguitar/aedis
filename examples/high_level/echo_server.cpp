/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <iostream>
#include <queue>
#include <vector>
#include <string>

#include <boost/asio/signal_set.hpp>

#include <aedis/aedis.hpp>
#include <aedis/src.hpp>

#include "user_session.hpp"

namespace net = boost::asio;
using aedis::redis::receiver_base;
using aedis::redis::command;
using aedis::resp3::node;
using aedis::generic::client;
using aedis::user_session;
using aedis::user_session_base;
using client_type = client<net::ip::tcp::socket, command>;
using response_type = std::vector<node<std::string>>;

class myreceiver : public receiver_base<response_type> {
private:
   std::queue<std::shared_ptr<user_session_base>> sessions_;

public:
   void on_read_impl(command cmd) override
   {
      switch (cmd) {
         case command::ping:
         sessions_.front()->deliver(get<response_type>().front().value);
         sessions_.pop();
         break;

         case command::incr:
         std::cout << "Echos so far: " << get<response_type>().front().value << std::endl;
         break;

         default: /* Ignore */;
      }

      get<response_type>().clear();
   }

   void add_user_session(std::shared_ptr<user_session_base> session)
      { sessions_.push(session); }
};

net::awaitable<void>
listener(
    std::shared_ptr<net::ip::tcp::acceptor> acc,
    std::shared_ptr<client_type> db,
    std::shared_ptr<myreceiver> recv)
{
   for (;;) {
      auto socket = co_await acc->async_accept(net::use_awaitable);
      auto session = std::make_shared<user_session>(std::move(socket));

      auto on_user_msg = [db, recv, session](std::string const& msg)
      {
         db->send(command::ping, msg);
         db->send(command::incr, "echo-counter");
         recv->add_user_session(session);
      };

      session->start(on_user_msg);
   }
}

int main()
{
   try {
      net::io_context ioc;

      auto db = std::make_shared<client_type>(ioc.get_executor());
      auto recv = std::make_shared<myreceiver>();

      db->async_run(
          *recv,
          {net::ip::make_address("127.0.0.1"), 6379},
          [](auto ec){ std::cout << ec.message() << std::endl;});

      auto endpoint = net::ip::tcp::endpoint{net::ip::tcp::v4(), 55555};
      auto acc = std::make_shared<net::ip::tcp::acceptor>(ioc.get_executor(), endpoint);
      co_spawn(ioc, listener(acc, db, recv), net::detached);

      net::signal_set signals(ioc.get_executor(), SIGINT, SIGTERM);
      signals.async_wait([&] (auto, int) { ioc.stop(); });

      ioc.run();
   } catch (std::exception const& e) {
      std::cerr << e.what() << std::endl;
   }
}
