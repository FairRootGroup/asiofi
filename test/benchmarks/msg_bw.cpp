/********************************************************************************
 * Copyright (C) 2018-2021 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH  *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>
#include <asio/buffer.hpp>
#include <asio/dispatch.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>
#include <asio/signal_set.hpp>
#include <asiofi.hpp>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <queue>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

auto handle_cli(int argc, char** argv, CLI::App& app) -> void
{
  app.add_option("host", "Host to bind on / connect to");
  app.add_flag  ("-v,--version", "Print version");
  app.add_option("-p,--port", "Server port")
               ->default_val(5000)
               ->check(CLI::PositiveNumber);
  app.add_flag  ("-s,--server", "Run server, otherwise client");
  app.add_option("-P,--provider", "Provider")
               ->default_str("sockets");
  app.add_option("-D,--domain", "Domain (HCA)")
               ->default_str("");
  app.add_option("-m,--message-size", "Message size in Byte")
               ->default_val(1024*1024)
               ->check(CLI::PositiveNumber);
  app.add_option("-i,--iterations", "Number of messages to transfer")
               ->default_val(100)
               ->check(CLI::PositiveNumber);
  app.add_option("-q,--queue-size", "Maximum number of transfers to queue in parallel")
               ->default_val(10)
               ->check(CLI::PositiveNumber);
  app.add_flag  ("--mt", "Multi-threaded mode");
  app.add_flag  ("--ctrl", "Emulate a control band");

  app.positionals_at_end(true);

  app.parse(argc, argv);

  if (app["--version"]->as<bool>()) {
    std::cout << "asiofi " << ASIOFI_GIT_VERSION << std::endl;
    std::exit(EXIT_SUCCESS);
  }
}

auto
  print_statistics(size_t message_size,
                   size_t iterations,
                   std::chrono::time_point<std::chrono::steady_clock> start,
                   std::chrono::time_point<std::chrono::steady_clock> stop) -> void
{
  auto elapsed_ms =
    std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
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
            const bool is_multi_threaded,
            const bool emulate_control_band) -> int
{
  asio::io_context io_context;
  asio::signal_set signals(io_context, SIGINT, SIGTERM);
  signals.async_wait([&](const std::error_code&, int) { io_context.stop(); });

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
  std::unique_ptr<asiofi::connected_endpoint> ctrl_endpoint(nullptr);

  size_t completed(0);
  size_t initiated(0);
  std::chrono::time_point<std::chrono::steady_clock> start;
  std::chrono::time_point<std::chrono::steady_clock> stop;

  asiofi::registered_memory_resource pmr(domain, message_size + 1024);
  const auto desc = pmr.get_region().desc();
  asio::mutable_buffer buffer(pmr.allocate(message_size), message_size);
  asio::mutable_buffer ctrl_buffer(pmr.allocate(1024), 1024);

  std::function<void()> post_buffers;
  std::function<void()> mt_main_thread;

  asiofi::unsynchronized_semaphore sem(io_context, queue_size);
  asiofi::unsynchronized_semaphore sem2(io_context, queue_size);

  asiofi::synchronized_semaphore queue_push(io_context, queue_size);
  asiofi::synchronized_semaphore queue_pop(io_context, 0);
  std::mutex queue_mtx;
  std::queue<asio::mutable_buffer> queue;

  if (is_server) {
      std::cout << "SERVER" << std::endl;
    if (is_multi_threaded) {
      std::cout << "MT" << std::endl;
    ////////////////////////
    // MT SERVER
      post_buffers = [&] {
        queue_pop.async_wait([&] {
          std::unique_lock<std::mutex> lk(queue_mtx);
          auto buffer2 = queue.front();
          queue.pop();
          lk.unlock();

          if (emulate_control_band) {
            ctrl_endpoint->send(ctrl_buffer, desc, [](asio::mutable_buffer) {});
          }

          endpoint->send(buffer2, desc, [&](asio::mutable_buffer) {
            if (completed == 0) {
              start = std::chrono::steady_clock::now();
            }
            ++completed;
            queue_push.signal();

            if (completed == iterations) {
              stop = std::chrono::steady_clock::now();
              endpoint->shutdown();
              signals.cancel();
              print_statistics(message_size, iterations, start, stop);
            }
          });

          asio::dispatch(io_context, post_buffers);
        });
      };

      mt_main_thread = [&] {
        while (initiated < iterations) {
          queue_push.wait();
          {
            std::unique_lock<std::mutex> lk(queue_mtx);
            queue.push(buffer);
          }
          queue_pop.signal();
          ++initiated;
        }
      };
    //
    ////////////////////////
    } else {
      std::cout << "ST" << std::endl;
    ////////////////////////
    // ST SERVER
      post_buffers = [&]() {
        sem.async_wait([&]() {
          if (emulate_control_band) {
            ctrl_endpoint->send(ctrl_buffer, desc, [](asio::mutable_buffer) {});
          }

          endpoint->send(buffer, desc, [&](asio::mutable_buffer) {
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
              print_statistics(message_size, iterations, start, stop);
            }
          });
          ++initiated;
          if (initiated < iterations) {
            asio::dispatch(io_context, post_buffers);
          }
        });
      };
    //
    ////////////////////////
    }

    // server listen logic
    pep = make_unique<asiofi::passive_endpoint>(io_context, fabric);
    pep->listen([&](asiofi::info&& info2) {
      std::cout << "listen1" << std::endl;
      endpoint = make_unique<asiofi::connected_endpoint>(io_context, domain, info2);
      endpoint->enable();
      endpoint->accept([&] {
        if (emulate_control_band) {
          pep->listen([&](asiofi::info&& info3) {
            std::cout << "listen2" << std::endl;
            ctrl_endpoint =
              make_unique<asiofi::connected_endpoint>(io_context, domain, info3);
            ctrl_endpoint->enable();
            ctrl_endpoint->accept([&] {
              std::cout << "accept2" << std::endl;
              asio::dispatch(io_context, post_buffers);
            });
          });
        } else {
          std::cout << "accept1" << std::endl;
          asio::dispatch(io_context, post_buffers);
        }
      });
    });
  } else {
      std::cout << "CLIENT" << std::endl;
    if (is_multi_threaded) {
    ////////////////////////
    // MT CLIENT
      throw std::runtime_error("--mt not yet implemented for client");
    //
    ////////////////////////
    } else {
      std::cout << "ST" << std::endl;
    ////////////////////////
    // ST CLIENT
      post_buffers = [&]() {
        sem.async_wait([&]() {
          if (initiated == 0) {
            start = std::chrono::steady_clock::now();
          }

          if (emulate_control_band) {
            ctrl_endpoint->recv(ctrl_buffer, desc, [&](asio::mutable_buffer) {
              sem.signal();
              sem2.async_wait([&]() {
                endpoint->recv(buffer, desc, [&](asio::mutable_buffer) {
                  ++completed;
                  sem2.signal();

                  if (completed == iterations) {
                    stop = std::chrono::steady_clock::now();
                    endpoint->shutdown();
                    signals.cancel();
                    print_statistics(message_size, iterations, start, stop);
                  }
                });
                ++initiated;
                if (initiated < iterations) {
                  asio::dispatch(io_context, post_buffers);
                }
              });
            });
          } else {
            endpoint->recv(buffer, desc, [&](asio::mutable_buffer) {
              ++completed;
              sem.signal();

              if (completed == iterations) {
                stop = std::chrono::steady_clock::now();
                endpoint->shutdown();
                signals.cancel();
                print_statistics(message_size, iterations, start, stop);
              }
            });
            ++initiated;
            if (initiated < iterations) {
              asio::dispatch(io_context, post_buffers);
            }
          }
        });
      };
    //
    ////////////////////////
    }

    // client connect logic
    endpoint = make_unique<asiofi::connected_endpoint>(io_context, domain);
    endpoint->enable();
    endpoint->connect([&](asiofi::eq::event e) {
      std::cout << "connect1" << std::endl;
      if (e == asiofi::eq::event::connected) {
        if (emulate_control_band) {
          ctrl_endpoint = make_unique<asiofi::connected_endpoint>(io_context, domain);
          ctrl_endpoint->enable();
          ctrl_endpoint->connect([&](asiofi::eq::event e2) {
            std::cout << "connect2" << std::endl;
            if (e2 == asiofi::eq::event::connected) {
              post_buffers();
            } else {
              throw std::runtime_error("Connection refused");
            }
          });
        } else {
          post_buffers();
        }
      } else {
        throw std::runtime_error("Connection refused");
      }
    });
  }

  // Run
  if (is_multi_threaded) {
    std::thread thread([&] {
      std::cout << "MT RUN IO THREAD" << std::endl;
      io_context.run();
    });
    std::cout << "MT RUN MAIN THREAD" << std::endl;
    mt_main_thread();
    thread.join();
  } else {
    std::cout << "ST RUN" << std::endl;
    io_context.run();
  }

  return EXIT_SUCCESS;
}

auto main(int argc, char** argv) -> int
{
  CLI::App app("Bandwidth test for MSG endpoints\n", "afi_msg_bw");

  try {
    handle_cli(argc, argv, app);
  } catch (CLI::ParseError const & e) {
    return app.exit(e);
  }

  return msg_bw(app["--server"]->as<bool>(),
                app["host"]->as<std::string>(),
                app["--port"]->as<std::string>(),
                app["--provider"]->as<std::string>(),
                app["--domain"]->as<std::string>(),
                app["--message-size"]->as<size_t>(),
                app["--iterations"]->as<size_t>(),
                app["--queue-size"]->as<size_t>(),
                app["--mt"]->as<bool>(),
                app["--ctrl"]->as<bool>());
}
