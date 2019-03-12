/********************************************************************************
 * Copyright (C) 2018-2019 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

// #define BOOST_ASIO_ENABLE_HANDLER_TRACKING
#include <asiofi.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace bpo = boost::program_options;

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

auto handle_cli(int argc, char** argv, bpo::variables_map& vm) -> void
try {
  bpo::options_description opts{"Options"};
  opts.add_options()
    ("help,h", "Help screen")
    ("version,v", "Print version")
    ("port,p", bpo::value<std::string>()->default_value("5000"), "Server port")
    ("server,s", "Run server, otherwise client")
    ("provider,P", bpo::value<std::string>()->default_value("sockets"), "Provider")
    ("domain,D", bpo::value<std::string>()->default_value(""), "Domain (HCA)")
    ("message-size,m", bpo::value<size_t>()->default_value(1024*1024), "Message size in Byte")
    ("iterations,i", bpo::value<size_t>()->default_value(100), "Number of messages to transfer")
    ("queue-size,q", bpo::value<size_t>()->default_value(10), "Maximum number of transfers to queue in parallel")
    ("mt", "Multi-threaded mode");
  
  bpo::options_description hidden;
  hidden.add_options()
    ("host", bpo::value<std::string>(), "Host to connect to");
  
  bpo::options_description all;
  all.add(opts).add(hidden);

  bpo::positional_options_description pos_opts;
  pos_opts.add("host", 1);

  bpo::store(bpo::command_line_parser(argc, argv).options(all).positional(pos_opts).run(), vm);
  bpo::notify(vm);

  if (vm.count("help")) {
    std::cout << "Usage:" << std::endl;
    std::cout << "  afi_msg_bw [OPTIONS] -s <ip>      Start server" << std::endl;
    std::cout << "  afi_msg_bw [OPTIONS] <ip>         Connect to server" << std::endl;
    std::cout << std::endl << "Bandwidth test for MSG endpoints." << std::endl << std::endl;
    std::cout << opts << std::endl;
    std::exit(EXIT_SUCCESS);
  } else if (vm.count("version")) {
    std::cout << "asiofi " << ASIOFI_GIT_VERSION << std::endl;
    std::exit(EXIT_SUCCESS);
  }
}
catch (const bpo::error& ex)
{
  std::cerr << ex.what() << std::endl;
  std::exit(EXIT_FAILURE);
}

auto print_statistics(size_t message_size, size_t iterations, double elapsed_ms) -> void
{
  auto rate_MiB = (iterations * message_size * 1000.) / (1024. * 1024. * elapsed_ms);
  auto rate_MB = (iterations * message_size * 1000.) / (1000. * 1000. * elapsed_ms);
  auto rate_Gb =
    (iterations * message_size * 1000. * 8.) / (1000. * 1000. * 1000. * elapsed_ms);
  auto sent_B = (iterations * message_size);
  auto sent_MB = sent_B / (1000. * 1000.);
  auto message_rate = (sent_B / message_size) * (1000. / elapsed_ms);
  std::cout << "  message size: " << message_size << " Bytes" << std::endl;
  std::cout << "    iterations: " << iterations << std::endl;
  std::cout << "  elapsed time: " << elapsed_ms << " ms" << std::endl;
  std::cout << "     data sent: " << sent_MB << " MB  " << sent_B << " Bytes" << std::endl;
  std::cout << "bandwidth used: " << rate_Gb << " Gb/s  " << rate_MiB << " MiB/s  "
            << rate_MB << " MB/s" << std::endl;
  std::cout << "  message rate: " << message_rate << " msg/s" << std::endl;
}

auto msg_bw(const bool is_server,
            const std::string& address,
            const std::string& port,
            const std::string& provider,
            const std::string& domain_str,
            size_t message_size,
            size_t iterations,
            size_t queue_size,
            const bool is_multi_threaded) -> int
{
  boost::asio::io_context io_context;
  boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](const boost::system::error_code& error, int signal_number) {
      io_context.stop();
  });

  asiofi::hints hints;
  hints.set_provider(provider);
  if (domain_str.length() > 0) hints.set_domain(domain_str);
  // std::cout << hints << std::endl;
  std::unique_ptr<asiofi::info> info(nullptr);
  if (is_server) {
    info = make_unique<asiofi::info>(address.c_str(), port.c_str(), FI_SOURCE, hints);
    //info->set_source(address, port);
  } else {
    //info = make_unique<asiofi::info>(address.c_str(), port.c_str(), 0, hints);
    info = make_unique<asiofi::info>(hints);
    info->set_destination(address, port);
  }
  // std::cout << info << std::endl;
  asiofi::fabric fabric(*info);
  asiofi::domain domain(fabric);
  std::unique_ptr<asiofi::passive_endpoint> pep(nullptr);
  std::unique_ptr<asiofi::connected_endpoint> endpoint(nullptr);

  asiofi::allocated_pool_resource pool_mr;
  size_t completed(0);
  size_t initiated(0);
  std::chrono::time_point<std::chrono::steady_clock> start;
  std::chrono::time_point<std::chrono::steady_clock> stop;

  boost::asio::mutable_buffer buffer(pool_mr.allocate(message_size), message_size);
  asiofi::memory_region mr(domain, buffer, asiofi::mr::access::recv);

  std::function<void()> post_buffers;

  if (is_multi_threaded) {
    asiofi::synchronized_semaphore queue_push(io_context, queue_size);
    asiofi::synchronized_semaphore queue_pop(io_context, 0);
    std::mutex queue_mtx;
    std::queue<boost::asio::mutable_buffer> queue;

    if (is_server) {
      ////////////////////////
      // MT SERVER

      pep = make_unique<asiofi::passive_endpoint>(io_context, fabric);
      pep->listen([&](asiofi::info&& info) {
        endpoint = make_unique<asiofi::connected_endpoint>(io_context, domain, info);
        endpoint->enable();
        endpoint->accept([&] { boost::asio::dispatch(io_context, post_buffers); });
      });

      post_buffers = [&] {
        queue_pop.async_wait([&] {
          std::unique_lock<std::mutex> lk(queue_mtx);
          auto buffer2 = queue.front();
          queue.pop();
          lk.unlock();

          endpoint->send(buffer2, mr.desc(), [&](boost::asio::mutable_buffer buffer3) {
            if (completed == 0) {
              start = std::chrono::steady_clock::now();
            }
            ++completed;
            queue_push.signal();

            if (completed == iterations) {
              stop = std::chrono::steady_clock::now();
              endpoint->shutdown();
              signals.cancel();
              auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(stop - start)
                  .count();
              print_statistics(message_size, iterations, elapsed_ms);
            }
          });

          boost::asio::dispatch(io_context, post_buffers);
        });
      };

      //
      ////////////////////////
    }

    std::thread thread([&] { io_context.run(); });

    while (initiated < iterations) {
      queue_push.wait();
      {
        std::unique_lock<std::mutex> lk(queue_mtx);
        queue.push(buffer);
      }
      queue_pop.signal();
      ++initiated;
    }

    thread.join();
  } else {
    asiofi::unsynchronized_semaphore sem(io_context, queue_size);

    if (is_server) {
      ////////////////////////
      // ST SERVER

      post_buffers = [&]() {
        sem.async_wait([&]() {
          endpoint->send(buffer, mr.desc(), [&](boost::asio::mutable_buffer buffer) {
            if (completed == 0) {
              start = std::chrono::steady_clock::now();
            }
            assert(buffer.size() == message_size);
            ++completed;
            sem.signal();

            if (completed == iterations) {
              stop = std::chrono::steady_clock::now();
              endpoint->shutdown();
              signals.cancel();
              auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(stop - start)
                  .count();
              print_statistics(message_size, iterations, elapsed_ms);
            }
          });
          ++initiated;
          if (initiated < iterations) {
            boost::asio::dispatch(io_context, post_buffers);
          }
        });
      };

      pep = make_unique<asiofi::passive_endpoint>(io_context, fabric);
      pep->listen([&](asiofi::info&& info) {
        endpoint = make_unique<asiofi::connected_endpoint>(io_context, domain, info);
        endpoint->enable();
        endpoint->accept([&]() { post_buffers(); });
      });

      //
      ////////////////////////
    } else {
      ////////////////////////
      // ST CLIENT

      post_buffers = [&]() {
        sem.async_wait([&]() {
          if (initiated == 0) {
            start = std::chrono::steady_clock::now();
          }
          endpoint->recv(buffer, mr.desc(), [&](boost::asio::mutable_buffer buffer) {
            ++completed;
            sem.signal();

            if (completed == iterations) {
              stop = std::chrono::steady_clock::now();
              endpoint->shutdown();
              signals.cancel();
              auto elapsed_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(stop - start)
                  .count();
              print_statistics(message_size, iterations, elapsed_ms);
            }
          });
          ++initiated;
          if (initiated < iterations) {
            boost::asio::dispatch(io_context, post_buffers);
          }
        });
      };

      endpoint = make_unique<asiofi::connected_endpoint>(io_context, domain);
      endpoint->enable();
      endpoint->connect([&](asiofi::eq::event e) {
        if (e == asiofi::eq::event::connected) {
          post_buffers();
        } else {
          throw std::runtime_error("Connection refused");
        }
      });

      //
      ////////////////////////
    }

    io_context.run();
  }

  return EXIT_SUCCESS;
}

auto main(int argc, char** argv) -> int
{
  bpo::variables_map vm;
  handle_cli(argc, argv, vm);

  return msg_bw(vm.count("server"),
                vm["host"].as<std::string>(),
                vm["port"].as<std::string>(),
                vm["provider"].as<std::string>(),
                vm["domain"].as<std::string>(),
                vm["message-size"].as<size_t>(),
                vm["iterations"].as<size_t>(),
                vm["queue-size"].as<size_t>(),
                vm.count("mt"));
}
