/********************************************************************************
 *    Copyright (C) 2018 GSI Helmholtzzentrum fuer Schwerionenforschung GmbH    *
 *                                                                              *
 *              This software is distributed under the terms of the             *
 *              GNU Lesser General Public Licence (LGPL) version 3,             *
 *                  copied verbatim in the file "LICENSE"                       *
 ********************************************************************************/

#ifndef ASIOFI_DETAIL_HANDLER_QUEUE_HPP
#define ASIOFI_DETAIL_HANDLER_QUEUE_HPP

#include <memory>
#include <queue>

namespace asiofi
{
namespace detail
{

struct queued_handler_base
{
  virtual ~queued_handler_base() { }
  virtual auto execute() -> void = 0;
  virtual auto release_context() -> std::unique_ptr<fi_context> = 0;
  virtual auto context() -> fi_context* = 0;
};

template<typename Handler>
struct queued_handler : queued_handler_base
{
  queued_handler(Handler handler, std::unique_ptr<fi_context> ctx)
  : m_handler(std::move(handler))
  , m_context(std::move(ctx))
  {
  }

  auto release_context() -> std::unique_ptr<fi_context> override
  {
    return std::move(m_context);
  }

  auto context() -> fi_context* override
  {
    return m_context.get();
  }

  auto execute() -> void override
  {
    m_handler();
  }

  private:
  Handler m_handler;
  std::unique_ptr<fi_context> m_context;
};

using handler_queue = std::queue<std::unique_ptr<queued_handler_base>>;

} /* namespace detail */
} /* namespace asiofi */

#endif /* ASIOFI_DETAIL_HANDLER_QUEUE_HPP */
