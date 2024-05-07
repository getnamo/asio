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
#include <iostream>
#include <memory>

using asio_sockio::ip::tcp;

class session : public std::enable_shared_from_this<session>
{
public:
  explicit session(tcp::socket socket)
    : socket_(std::move(socket)),
      timer_(socket_.get_io_context()),
      strand_(socket_.get_io_context())
  {
  }

  void go()
  {
    auto self(shared_from_this());
    asio_sockio::spawn(strand_,
        [this, self](asio_sockio::yield_context yield)
        {
          try
          {
            char data[128];
            for (;;)
            {
              timer_.expires_from_now(std::chrono::seconds(10));
              std::size_t n = socket_.async_read_some(asio_sockio::buffer(data), yield);
              asio_sockio::async_write(socket_, asio_sockio::buffer(data, n), yield);
            }
          }
          catch (std::exception& e)
          {
            socket_.close();
            timer_.cancel();
          }
        });

    asio_sockio::spawn(strand_,
        [this, self](asio_sockio::yield_context yield)
        {
          while (socket_.is_open())
          {
            asio_sockio::error_code ignored_ec;
            timer_.async_wait(yield[ignored_ec]);
            if (timer_.expires_from_now() <= std::chrono::seconds(0))
              socket_.close();
          }
        });
  }

private:
  tcp::socket socket_;
  asio_sockio::steady_timer timer_;
  asio_sockio::io_context::strand strand_;
};

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
        [&](asio_sockio::yield_context yield)
        {
          tcp::acceptor acceptor(io_context,
            tcp::endpoint(tcp::v4(), std::atoi(argv[1])));

          for (;;)
          {
            asio_sockio::error_code ec;
            tcp::socket socket(io_context);
            acceptor.async_accept(socket, yield[ec]);
            if (!ec) std::make_shared<session>(std::move(socket))->go();
          }
        });

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
