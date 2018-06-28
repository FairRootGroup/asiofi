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
  virtual void execute() = 0;
};

template<typename Handler>
struct queued_handler : queued_handler_base
{
  queued_handler(Handler handler)
  : m_handler(std::move(handler))
  {
  }

  auto execute() -> void override
  {
    m_handler();
  }

  private:
  Handler m_handler;
};

using handler_queue = std::queue<std::unique_ptr<queued_handler_base>>;

} /* namespace detail */
} /* namespace asiofi */

#endif /* ASIOFI_DETAIL_HANDLER_QUEUE_HPP */
