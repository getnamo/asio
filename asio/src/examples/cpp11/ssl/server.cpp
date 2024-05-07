//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <functional>
#include <iostream>
#include "asio.hpp"
#include "asio/ssl.hpp"

using asio_sockio::ip::tcp;

class session : public std::enable_shared_from_this<session>
{
public:
  session(tcp::socket socket, asio_sockio::ssl::context& context)
    : socket_(std::move(socket), context)
  {
  }

  void start()
  {
    do_handshake();
  }

private:
  void do_handshake()
  {
    auto self(shared_from_this());
    socket_.async_handshake(asio_sockio::ssl::stream_base::server, 
        [this, self](const std::error_code& error)
        {
          if (!error)
          {
            do_read();
          }
        });
  }

  void do_read()
  {
    auto self(shared_from_this());
    socket_.async_read_some(asio_sockio::buffer(data_),
        [this, self](const std::error_code& ec, std::size_t length)
        {
          if (!ec)
          {
            do_write(length);
          }
        });
  }

  void do_write(std::size_t length)
  {
    auto self(shared_from_this());
    asio_sockio::async_write(socket_, asio_sockio::buffer(data_, length),
        [this, self](const std::error_code& ec,
          std::size_t /*length*/)
        {
          if (!ec)
          {
            do_read();
          }
        });
  }

  asio_sockio::ssl::stream<tcp::socket> socket_;
  char data_[1024];
};

class server
{
public:
  server(asio_sockio::io_context& io_context, unsigned short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
      context_(asio_sockio::ssl::context::sslv23)
  {
    context_.set_options(
        asio_sockio::ssl::context::default_workarounds
        | asio_sockio::ssl::context::no_sslv2
        | asio_sockio::ssl::context::single_dh_use);
    context_.set_password_callback(std::bind(&server::get_password, this));
    context_.use_certificate_chain_file("server.pem");
    context_.use_private_key_file("server.pem", asio_sockio::ssl::context::pem);
    context_.use_tmp_dh_file("dh2048.pem");

    do_accept();
  }

private:
  std::string get_password() const
  {
    return "test";
  }

  void do_accept()
  {
    acceptor_.async_accept(
        [this](const std::error_code& error, tcp::socket socket)
        {
          if (!error)
          {
            std::make_shared<session>(std::move(socket), context_)->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
  asio_sockio::ssl::context context_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 2)
    {
      std::cerr << "Usage: server <port>\n";
      return 1;
    }

    asio_sockio::io_context io_context;

    using namespace std; // For atoi.
    server s(io_context, atoi(argv[1]));

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
