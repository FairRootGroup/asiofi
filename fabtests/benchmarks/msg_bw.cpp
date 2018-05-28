/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#include <asiofi/fabric.hpp>
#include <asiofi/version.hpp>
#include <boost/program_options.hpp>
#include <benchmark/benchmark.h>
#include <cstdlib>
#include <iostream>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <string>
#include <sys/mman.h>

namespace bpo = boost::program_options;

auto bm_naive(benchmark::State& state) -> void
{
  const size_t size = state.range(0);
  const auto step = sizeof(size_t);

  for (auto _ : state) {
    const auto x = static_cast<size_t*>(std::malloc(size));
    const auto end = x + size / sizeof(size_t);
    for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
    std::free(x);
  }

  state.SetBytesProcessed(state.iterations() * size);
}

auto bm_naive_hp(benchmark::State& state) -> void
{
  const size_t size = state.range(0);
  const auto step = sizeof(size_t);

  for (auto _ : state) {
    const auto x = static_cast<size_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0));
    if (x != MAP_FAILED) {
      const auto end = x + size / sizeof(size_t);
      for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
      munmap(static_cast<void*>(x), size);
    } else {
      std::cerr << "Allocation failed." << std::endl;
      std::exit(EXIT_FAILURE);
    }
  }

  state.SetBytesProcessed(state.iterations() * size);
}

auto bm_reuse(benchmark::State& state) -> void
{
  const size_t size = state.range(0);
  const auto step = sizeof(size_t);
  const auto x = static_cast<size_t*>(std::malloc(size));
  const auto end = x + size / sizeof(size_t);
  for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();

  for (auto _ : state) {
    for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
  }

  std::free(x);
  state.SetBytesProcessed(state.iterations() * size);
}

auto bm_reuse_hp(benchmark::State& state) -> void
{
  const size_t size = state.range(0);
  const auto step = sizeof(size_t);
  const auto x = static_cast<size_t*>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_HUGETLB, 0, 0));
  if (x != MAP_FAILED) {
    const auto end = x + size / sizeof(size_t);
    for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();

    for (auto _ : state) {
      for (auto it = x; it < end; it += step / sizeof(size_t)) *it = size_t();
    }

    munmap(static_cast<void*>(x), size);
    state.SetBytesProcessed(state.iterations() * size);
  } else {
    std::cerr << "Allocation failed." << std::endl;
    std::exit(EXIT_FAILURE);
  }
}
auto handle_cli(int argc, char** argv, bpo::variables_map& vm) -> void
try {
  bpo::options_description opts{"Options"};
  opts.add_options()
    ("help,h", "Help screen")
    ("version,v", "Print version")
    ("memory", "Run local memory benchmark");
  
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
    std::cout << "  afi_msg_bw [OPTIONS]                start server" << std::endl;
    std::cout << "  afi_msg_bw [OPTIONS] <host>         connect to server" << std::endl;
    std::cout << std::endl << "Bandwidth test for MSG endpoints." << std::endl << std::endl;
    std::cout << opts << std::endl;
    std::exit(EXIT_SUCCESS);
  } else if (vm.count("version")) {
    std::cout << "asiofi " << ASIOFI_GIT_VERSION << std::endl;
    std::exit(EXIT_SUCCESS);
  } else if (vm.count("memory")) {
    benchmark::Initialize(&argc, argv);

    benchmark::RegisterBenchmark("naive", &bm_naive)->
      RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
    benchmark::RegisterBenchmark("naive_hp", &bm_naive_hp)->
      RangeMultiplier(2)->Range(1<<21, 1<<29)->Threads(1);
    benchmark::RegisterBenchmark("reuse", &bm_reuse)->
      RangeMultiplier(2)->Range(1<<12, 1<<29)->Threads(1);
    benchmark::RegisterBenchmark("reuse_hp", &bm_reuse_hp)->
      RangeMultiplier(2)->Range(1<<21, 1<<29)->Threads(1);

    benchmark::RunSpecifiedBenchmarks();
    std::exit(EXIT_SUCCESS);
  }
}
catch (const bpo::error& ex)
{
  std::cerr << ex.what() << std::endl;
  std::exit(EXIT_FAILURE);
}

auto main(int argc, char** argv) -> int
{
  bpo::variables_map vm;
  handle_cli(argc, argv, vm);

  asiofi::hints hints;
  asiofi::info info(0, hints);
  // std::cout << info << std::endl;

  // fid_fabric* fabric;
  // fid_domain* domain;
  // fid_av* av;
  // fid_eq* eq;
  // fid_ep* ep;
  // fid_cq* cq_tx;
  // fid_cq* cq_rx;
  // fi_context ctx;
//
  // sockaddr_in* sa = static_cast<sockaddr_in*>(malloc(sizeof(sockaddr_in)));
  // addr.Port = 0;
  // auto sa2 = ConvertAddress(addr);
  // memcpy(sa, &sa2, sizeof(sockaddr_in));
//
  //Prepare fi_getinfo query
  // unique_ptr<fi_info, void(*)(fi_info*)> hints(fi_allocinfo(), fi_freeinfo);
  // hints->caps = FI_MSG;
  //ofi_hints->mode = FI_CONTEXT;
  // hints->addr_format = FI_SOCKADDR_IN;
  // if (addr.Protocol == "tcp") {
      // hints->fabric_attr->prov_name = strdup("sockets");
  // } else if (addr.Protocol == "verbs") {
      // hints->fabric_attr->prov_name = strdup("verbs;ofi_rxm");
  // }
  // hints->ep_attr->type = FI_EP_RDM;
  //ofi_hints->domain_attr->mr_mode = FI_MR_BASIC | FI_MR_SCALABLE;
  // hints->domain_attr->threading = FI_THREAD_SAFE;
  // hints->domain_attr->control_progress = FI_PROGRESS_AUTO;
  // hints->domain_attr->data_progress = FI_PROGRESS_AUTO;
  // hints->tx_attr->op_flags = FI_COMPLETION;
  // hints->rx_attr->op_flags = FI_COMPLETION;
  // if (vm.count("host")) {
      // ofi_hints->src_addr = sa;
      // ofi_hints->src_addrlen = sizeof(sockaddr_in);
      // ofi_hints->dest_addr = nullptr;
      // ofi_hints->dest_addrlen = 0;
  // } else {
      // ofi_hints->src_addr = nullptr;
      // ofi_hints->src_addrlen = 0;
      // ofi_hints->dest_addr = sa;
      // ofi_hints->dest_addrlen = sizeof(sockaddr_in);
  // }
  // auto ret = fi_getinfo(FI_VERSION(1, 5), nullptr, nullptr, 0, hints, &info);
  // if (ret != 0)
    // std::cerr < "Failed querying fi_getinfo, reason: " << fi_strerror(res) << std::endl;
//
  // ret = fi_fabric(info->fabric_attr, &fabric, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed opening ofi fabric, reason: " << fi_strerror(ret) << std::endl;
//
  // ret = fi_domain(fabric, info, &domain, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed opening ofi domain, reason: " << fi_strerror(ret) << std::endl;
//
  // fi_eq_attr eq_attr = {100, 0, FI_WAIT_UNSPEC, 0, nullptr};
  // size_t               size;      [> # entries for EQ <]
  // uint64_t             flags;     [> operation flags <]
  // enum fi_wait_obj     wait_obj;  [> requested wait object <]
  // int                  signaling_vector; [> interrupt affinity <]
  // struct fid_wait     *wait_set;  [> optional wait set <]
  // ret = fi_eq_open(fabric, &eq_attr, &eq, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed opening ofi event queue, reason: " << fi_strerror(ret) << std::endl;
//
  // fi_av_attr av_attr = {info->domain_attr->av_type, 0, 1000, 0, nullptr, nullptr, 0};
  // enum fi_av_type  type;        [> type of AV <]
  // int              rx_ctx_bits; [> address bits to identify rx ctx <]
  // size_t           count;       [> # entries for AV <]
  // size_t           ep_per_node; [> # endpoints per fabric address <]
  // const char       *name;       [> system name of AV <]
  // void             *map_addr;   [> base mmap address <]
  // uint64_t         flags;       [> operation flags <]
  // ret = fi_av_open(domain, &av_attr, &av, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed opening ofi address vector, reason: " << fi_strerror(ret) << std::endl;
//
  // ret = fi_endpoint(domain, info, &ep, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed creating ofi endpoint, reason: " << fi_strerror(ret) << std::endl;
//
  // ret = fi_ep_bind(ep, &av->fid, 0);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed binding ofi address vector to ofi endpoint, reason: " << fi_strerror(ret) << std::endl;
//
  // fi_cq_attr cq_attr_rx = {0, 0, FI_CQ_FORMAT_DATA, FI_WAIT_UNSPEC, 0, FI_CQ_COND_NONE, nullptr};
  // cq_attr_rx.size = info->rx_attr->size;
  // fi_cq_attr cq_attr_tx = {0, 0, FI_CQ_FORMAT_DATA, FI_WAIT_UNSPEC, 0, FI_CQ_COND_NONE, nullptr};
  // cq_attr_tx.size = info->tx_attr->size;
  // size_t               size;      [> # entries for CQ <]
  // uint64_t             flags;     [> operation flags <]
  // enum fi_cq_format    format;    [> completion format <]
  // enum fi_wait_obj     wait_obj;  [> requested wait object <]
  // int                  signaling_vector; [> interrupt affinity <]
  // enum fi_cq_wait_cond wait_cond; [> wait condition format <]
  // struct fid_wait     *wait_set;  [> optional wait set <]
  // ret = fi_cq_open(fdomain, &attr_rx, &cq_rx, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed creating ofi RX completion queue, reason: " << fi_strerror(ret) << std::endl;
  // ret = fi_cq_open(fdomain, &attr_tx, &cq_tx, &ctx);
  // if (ret != FI_SUCCESS)
    // std::cerr << "Failed creating ofi TX completion queue, reason: " << fi_strerror(ret) << std::endl;
//
  // if (vm.count("host")) {
    // std::cout << "client connecting to " << vm["host"].as<std::string>() << std::endl;
  // } else {
    // std::cout << "server" << std::endl;
  // }
//
	// if (eq) {
    // auto ret = fi_close(&eq->fid);
    // if (ret != FI_SUCCESS)
      // std::cerr << "Failed closing ofi event queue, reason: " << fi_strerror(ret) << std::endl;
  // }
//
	// if (av) {
    // auto ret = fi_close(&av->fid);
    // if (ret != FI_SUCCESS)
      // std::cerr << "Failed closing ofi address vector, reason: " << fi_strerror(ret) << std::endl;
  // }
//
	// if (domain) {
    // auto ret = fi_close(&domain->fid);
    // if (ret != FI_SUCCESS)
      // std::cerr << "Failed closing ofi domain, reason: " << fi_strerror(ret) << std::endl;
  // }
//
	// if (fabric) {
    // auto ret = fi_close(&fabric->fid);
    // if (ret != FI_SUCCESS)
      // std::cerr << "Failed closing ofi fabric, reason: " << fi_strerror(ret) << std::endl;
  // }

  return EXIT_SUCCESS;
}
