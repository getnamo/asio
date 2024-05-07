//
// client.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include "asio.hpp"
#include "asio/ssl.hpp"

enum { max_length = 1024 };

class client
{
public:
  client(asio_sockio::io_context& io_context,
      asio_sockio::ssl::context& context,
      asio_sockio::ip::tcp::resolver::results_type endpoints)
    : socket_(io_context, context)
  {
    socket_.set_verify_mode(asio_sockio::ssl::verify_peer);
    socket_.set_verify_callback(
        boost::bind(&client::verify_certificate, this, _1, _2));

    asio_sockio::async_connect(socket_.lowest_layer(), endpoints,
        boost::bind(&client::handle_connect, this,
          asio_sockio::placeholders::error));
  }

  bool verify_certificate(bool preverified,
      asio_sockio::ssl::verify_context& ctx)
  {
    // The verify callback can be used to check whether the certificate that is
    // being presented is valid for the peer. For example, RFC 2818 describes
    // the steps involved in doing this for HTTPS. Consult the OpenSSL
    // documentation for more details. Note that the callback is called once
    // for each certificate in the certificate chain, starting from the root
    // certificate authority.

    // In this example we will simply print the certificate's subject name.
    char subject_name[256];
    X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
    X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);
    std::cout << "Verifying " << subject_name << "\n";

    return preverified;
  }

  void handle_connect(const asio_sockio::error_code& error)
  {
    if (!error)
    {
      socket_.async_handshake(asio_sockio::ssl::stream_base::client,
          boost::bind(&client::handle_handshake, this,
            asio_sockio::placeholders::error));
    }
    else
    {
      std::cout << "Connect failed: " << error.message() << "\n";
    }
  }

  void handle_handshake(const asio_sockio::error_code& error)
  {
    if (!error)
    {
      std::cout << "Enter message: ";
      std::cin.getline(request_, max_length);
      size_t request_length = strlen(request_);

      asio_sockio::async_write(socket_,
          asio_sockio::buffer(request_, request_length),
          boost::bind(&client::handle_write, this,
            asio_sockio::placeholders::error,
            asio_sockio::placeholders::bytes_transferred));
    }
    else
    {
      std::cout << "Handshake failed: " << error.message() << "\n";
    }
  }

  void handle_write(const asio_sockio::error_code& error,
      size_t bytes_transferred)
  {
    if (!error)
    {
      asio_sockio::async_read(socket_,
          asio_sockio::buffer(reply_, bytes_transferred),
          boost::bind(&client::handle_read, this,
            asio_sockio::placeholders::error,
            asio_sockio::placeholders::bytes_transferred));
    }
    else
    {
      std::cout << "Write failed: " << error.message() << "\n";
    }
  }

  void handle_read(const asio_sockio::error_code& error,
      size_t bytes_transferred)
  {
    if (!error)
    {
      std::cout << "Reply: ";
      std::cout.write(reply_, bytes_transferred);
      std::cout << "\n";
    }
    else
    {
      std::cout << "Read failed: " << error.message() << "\n";
    }
  }

private:
  asio_sockio::ssl::stream<asio_sockio::ip::tcp::socket> socket_;
  char request_[max_length];
  char reply_[max_length];
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: client <host> <port>\n";
      return 1;
    }

    asio_sockio::io_context io_context;

    asio_sockio::ip::tcp::resolver resolver(io_context);
    asio_sockio::ip::tcp::resolver::results_type endpoints =
      resolver.resolve(argv[1], argv[2]);

    asio_sockio::ssl::context ctx(asio_sockio::ssl::context::sslv23);
    ctx.load_verify_file("ca.pem");

    client c(io_context, ctx, endpoints);

    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
