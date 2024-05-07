//
// client.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2019 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "asio.hpp"
#include <algorithm>
#include <boost/bind.hpp>
#include <boost/mem_fn.hpp>
#include <iostream>
#include <list>
#include <string>
#include "handler_allocator.hpp"

class stats
{
public:
  stats()
    : mutex_(),
      total_bytes_written_(0),
      total_bytes_read_(0)
  {
  }

  void add(size_t bytes_written, size_t bytes_read)
  {
    asio_sockio::detail::mutex::scoped_lock lock(mutex_);
    total_bytes_written_ += bytes_written;
    total_bytes_read_ += bytes_read;
  }

  void print()
  {
    asio_sockio::detail::mutex::scoped_lock lock(mutex_);
    std::cout << total_bytes_written_ << " total bytes written\n";
    std::cout << total_bytes_read_ << " total bytes read\n";
  }

private:
  asio_sockio::detail::mutex mutex_;
  size_t total_bytes_written_;
  size_t total_bytes_read_;
};

class session
{
public:
  session(asio_sockio::io_context& ioc, size_t block_size, stats& s)
    : strand_(ioc),
      socket_(ioc),
      block_size_(block_size),
      read_data_(new char[block_size]),
      read_data_length_(0),
      write_data_(new char[block_size]),
      unwritten_count_(0),
      bytes_written_(0),
      bytes_read_(0),
      stats_(s)
  {
    for (size_t i = 0; i < block_size_; ++i)
      write_data_[i] = static_cast<char>(i % 128);
  }

  ~session()
  {
    stats_.add(bytes_written_, bytes_read_);

    delete[] read_data_;
    delete[] write_data_;
  }

  void start(asio_sockio::ip::tcp::resolver::results_type endpoints)
  {
    asio_sockio::async_connect(socket_, endpoints,
        asio_sockio::bind_executor(strand_,
          boost::bind(&session::handle_connect, this,
            asio_sockio::placeholders::error)));
  }

  void stop()
  {
    asio_sockio::post(strand_, boost::bind(&session::close_socket, this));
  }

private:
  void handle_connect(const asio_sockio::error_code& err)
  {
    if (!err)
    {
      asio_sockio::error_code set_option_err;
      asio_sockio::ip::tcp::no_delay no_delay(true);
      socket_.set_option(no_delay, set_option_err);
      if (!set_option_err)
      {
        ++unwritten_count_;
        async_write(socket_, asio_sockio::buffer(write_data_, block_size_),
            asio_sockio::bind_executor(strand_,
              make_custom_alloc_handler(write_allocator_,
                boost::bind(&session::handle_write, this,
                  asio_sockio::placeholders::error,
                  asio_sockio::placeholders::bytes_transferred))));
        socket_.async_read_some(asio_sockio::buffer(read_data_, block_size_),
            asio_sockio::bind_executor(strand_,
              make_custom_alloc_handler(read_allocator_,
                boost::bind(&session::handle_read, this,
                  asio_sockio::placeholders::error,
                  asio_sockio::placeholders::bytes_transferred))));
      }
    }
  }

  void handle_read(const asio_sockio::error_code& err, size_t length)
  {
    if (!err)
    {
      bytes_read_ += length;

      read_data_length_ = length;
      ++unwritten_count_;
      if (unwritten_count_ == 1)
      {
        std::swap(read_data_, write_data_);
        async_write(socket_, asio_sockio::buffer(write_data_, read_data_length_),
            asio_sockio::bind_executor(strand_,
              make_custom_alloc_handler(write_allocator_,
                boost::bind(&session::handle_write, this,
                  asio_sockio::placeholders::error,
                  asio_sockio::placeholders::bytes_transferred))));
        socket_.async_read_some(asio_sockio::buffer(read_data_, block_size_),
            asio_sockio::bind_executor(strand_,
              make_custom_alloc_handler(read_allocator_,
                boost::bind(&session::handle_read, this,
                  asio_sockio::placeholders::error,
                  asio_sockio::placeholders::bytes_transferred))));
      }
    }
  }

  void handle_write(const asio_sockio::error_code& err, size_t length)
  {
    if (!err && length > 0)
    {
      bytes_written_ += length;

      --unwritten_count_;
      if (unwritten_count_ == 1)
      {
        std::swap(read_data_, write_data_);
        async_write(socket_, asio_sockio::buffer(write_data_, read_data_length_),
            asio_sockio::bind_executor(strand_,
              make_custom_alloc_handler(write_allocator_,
                boost::bind(&session::handle_write, this,
                  asio_sockio::placeholders::error,
                  asio_sockio::placeholders::bytes_transferred))));
        socket_.async_read_some(asio_sockio::buffer(read_data_, block_size_),
            asio_sockio::bind_executor(strand_,
              make_custom_alloc_handler(read_allocator_,
                boost::bind(&session::handle_read, this,
                  asio_sockio::placeholders::error,
                  asio_sockio::placeholders::bytes_transferred))));
      }
    }
  }

  void close_socket()
  {
    socket_.close();
  }

private:
  asio_sockio::io_context::strand strand_;
  asio_sockio::ip::tcp::socket socket_;
  size_t block_size_;
  char* read_data_;
  size_t read_data_length_;
  char* write_data_;
  int unwritten_count_;
  size_t bytes_written_;
  size_t bytes_read_;
  stats& stats_;
  handler_allocator read_allocator_;
  handler_allocator write_allocator_;
};

class client
{
public:
  client(asio_sockio::io_context& ioc,
      const asio_sockio::ip::tcp::resolver::results_type endpoints,
      size_t block_size, size_t session_count, int timeout)
    : io_context_(ioc),
      stop_timer_(ioc),
      sessions_(),
      stats_()
  {
    stop_timer_.expires_after(asio_sockio::chrono::seconds(timeout));
    stop_timer_.async_wait(boost::bind(&client::handle_timeout, this));

    for (size_t i = 0; i < session_count; ++i)
    {
      session* new_session = new session(io_context_, block_size, stats_);
      new_session->start(endpoints);
      sessions_.push_back(new_session);
    }
  }

  ~client()
  {
    while (!sessions_.empty())
    {
      delete sessions_.front();
      sessions_.pop_front();
    }

    stats_.print();
  }

  void handle_timeout()
  {
    std::for_each(sessions_.begin(), sessions_.end(),
        boost::mem_fn(&session::stop));
  }

private:
  asio_sockio::io_context& io_context_;
  asio_sockio::steady_timer stop_timer_;
  std::list<session*> sessions_;
  stats stats_;
};

int main(int argc, char* argv[])
{
  try
  {
    if (argc != 7)
    {
      std::cerr << "Usage: client <host> <port> <threads> <blocksize> ";
      std::cerr << "<sessions> <time>\n";
      return 1;
    }

    using namespace std; // For atoi.
    const char* host = argv[1];
    const char* port = argv[2];
    int thread_count = atoi(argv[3]);
    size_t block_size = atoi(argv[4]);
    size_t session_count = atoi(argv[5]);
    int timeout = atoi(argv[6]);

    asio_sockio::io_context ioc;

    asio_sockio::ip::tcp::resolver r(ioc);
    asio_sockio::ip::tcp::resolver::results_type endpoints =
      r.resolve(host, port);

    client c(ioc, endpoints, block_size, session_count, timeout);

    std::list<asio_sockio::thread*> threads;
    while (--thread_count > 0)
    {
      asio_sockio::thread* new_thread = new asio_sockio::thread(
          boost::bind(&asio_sockio::io_context::run, &ioc));
      threads.push_back(new_thread);
    }

    ioc.run();

    while (!threads.empty())
    {
      threads.front()->join();
      delete threads.front();
      threads.pop_front();
    }
  }
  catch (std::exception& e)
  {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
