//
// receiver.cpp
// ~~~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <string>
#include "asio.hpp"
#include "boost/bind.hpp"

const short multicast_port = 30001;

class receiver
{
public:
  receiver(asio_sockio::io_context& io_context,
      const asio_sockio::ip::address& listen_address,
      const asio_sockio::ip::address& multicast_address)
    : socket_(io_context)
  {
    // Create the socket so that multiple may be bound to the same address.
    asio_sockio::ip::udp::endpoint listen_endpoint(
        listen_address, multicast_port);
    socket_.open(listen_endpoint.protocol());
    socket_.set_option(asio_sockio::ip::udp::socket::reuse_address(true));
    socket_.bind(listen_endpoint);

    // Join the multicast group.
    socket_.set_option(
        asio_sockio::ip::multicast::join_group(multicast_address));

    socket_.async_receive_from(
        asio_sockio::buffer(data_, max_length), sender_endpoint_,
        boost::bind(&receiver::handle_receive_from, this,
          asio_sockio::placeholders::error,
          asio_sockio::placeholders::bytes_transferred));
  }

  void handle_receive_from(const asio_sockio::error_code& error,
      size_t bytes_recvd)
  {
    if (!error)
    {
      std::cout.write(data_, bytes_recvd);
      std::cout << std::endl;

      socket_.async_receive_from(
          asio_sockio::buffer(data_, max_length), sender_endpoint_,
          boost::bind(&receiver::handle_receive_from, this,
            asio_sockio::placeholders::error,
            asio_sockio::placeholders::bytes_transferred));
    }
  }

private:
  asio_sockio::ip::udp::socket socket_;
  asio_sockio::ip::udp::endpoint sender_endpoint_;
  enum { max_length = 1024 };
  char data_[max_length];
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 3)
    {
      std::cerr << "Usage: receiver <listen_address> <multicast_address>\n";
      std::cerr << "  For IPv4, try:\n";
      std::cerr << "    receiver 0.0.0.0 239.255.0.1\n";
      std::cerr << "  For IPv6, try:\n";
      std::cerr << "    receiver 0::0 ff31::8000:1234\n";
      return 1;
    }

    asio_sockio::io_context io_context;
    receiver r(io_context,
        asio_sockio::ip::make_address(argv[1]),
        asio_sockio::ip::make_address(argv[2]));
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
