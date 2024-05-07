//
// echo_server.cpp
// ~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <asio/io_context.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/spawn.hpp>
#include <asio/steady_timer.hpp>
#include <asio/write.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <iostream>

using asio_sockio::ip::tcp;

class session : public boost::enable_shared_from_this<session>
{
public:
  explicit session(asio_sockio::io_context& io_context)
    : strand_(io_context),
      socket_(io_context),
      timer_(io_context)
  {
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void go()
  {
    asio_sockio::spawn(strand_,
        boost::bind(&session::echo,
          shared_from_this(), _1));
    asio_sockio::spawn(strand_,
        boost::bind(&session::timeout,
          shared_from_this(), _1));
  }

private:
  void echo(asio_sockio::yield_context yield)
  {
    try
    {
      char data[128];
      for (;;)
      {
        timer_.expires_after(asio_sockio::chrono::seconds(10));
        std::size_t n = socket_.async_read_some(asio_sockio::buffer(data), yield);
        asio_sockio::async_write(socket_, asio_sockio::buffer(data, n), yield);
      }
    }
    catch (std::exception& e)
    {
      socket_.close();
      timer_.cancel();
    }
  }

  void timeout(asio_sockio::yield_context yield)
  {
    while (socket_.is_open())
    {
      asio_sockio::error_code ignored_ec;
      timer_.async_wait(yield[ignored_ec]);
      if (timer_.expiry() <= asio_sockio::steady_timer::clock_type::now())
        socket_.close();
    }
  }

  asio_sockio::io_context::strand strand_;
  tcp::socket socket_;
  asio_sockio::steady_timer timer_;
};

void do_accept(asio_sockio::io_context& io_context,
    unsigned short port, asio_sockio::yield_context yield)
{
  tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), port));

  for (;;)
  {
    asio_sockio::error_code ec;
    boost::shared_ptr<session> new_session(new session(io_context));
    acceptor.async_accept(new_session->socket(), yield[ec]);
    if (!ec) new_session->go();
  }
}

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: echo_server <port>\n";
      return 1;
    }

    asio_sockio::io_context io_context;

    asio_sockio::spawn(io_context,
        boost::bind(do_accept,
          boost::ref(io_context), atoi(argv[1]), _1));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
