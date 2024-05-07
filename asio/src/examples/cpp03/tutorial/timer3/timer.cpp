//
// timer.cpp
// ~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <iostream>
#include <asio.hpp>
#include <boost/bind.hpp>

void print(const asio_sockio::error_code& /*e*/,
    asio_sockio::steady_timer* t, int* count)
{
  if (*count < 5)
  {
    std::cout << *count << std::endl;
    ++(*count);

    t->expires_at(t->expiry() + asio_sockio::chrono::seconds(1));
    t->async_wait(boost::bind(print,
          asio_sockio::placeholders::error, t, count));
  }
}

int main()
{
  asio_sockio::io_context io;

  int count = 0;
  asio_sockio::steady_timer t(io, asio_sockio::chrono::seconds(1));
  t.async_wait(boost::bind(print,
        asio_sockio::placeholders::error, &t, &count));

  io.run();

  std::cout << "Final count is " << count << std::endl;

  return 0;
}
