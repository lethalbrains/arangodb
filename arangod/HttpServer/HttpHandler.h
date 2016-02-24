////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_HTTP_SERVER_HTTP_HANDLER_H
#define ARANGOD_HTTP_SERVER_HTTP_HANDLER_H 1

#include "Basics/Common.h"

#include "Basics/Exceptions.h"
#include "Basics/WorkMonitor.h"
#include "Dispatcher/Job.h"
#include "Rest/HttpResponse.h"
#include "Scheduler/events.h"
#include "Statistics/StatisticsAgent.h"

namespace arangodb {
namespace rest {
class Dispatcher;
class HttpHandlerFactory;
class HttpRequest;

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract class for http handlers
////////////////////////////////////////////////////////////////////////////////

class HttpHandler : public RequestStatisticsAgent, public arangodb::WorkItem {
  HttpHandler(HttpHandler const&) = delete;
  HttpHandler& operator=(HttpHandler const&) = delete;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a new handler
  ///
  /// Note that the handler owns the request and the response. It is its
  /// responsibility to destroy them both. See also the two steal methods.
  //////////////////////////////////////////////////////////////////////////////

  explicit HttpHandler(HttpRequest*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destructs a handler
  //////////////////////////////////////////////////////////////////////////////

 protected:
  ~HttpHandler();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief status of execution
  //////////////////////////////////////////////////////////////////////////////

  enum status_e { HANDLER_DONE, HANDLER_FAILED, HANDLER_ASYNC };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief result of execution
  //////////////////////////////////////////////////////////////////////////////

  class status_t {
   public:
    status_t() : status_t(HANDLER_FAILED) {}
    explicit status_t(status_e status) : _status(status) {}
    void operator=(status_e status) { _status = status; }
    status_e _status;
  };

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns true if a handler is executed directly
  //////////////////////////////////////////////////////////////////////////////

  virtual bool isDirect() const = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the queue name
  //////////////////////////////////////////////////////////////////////////////

  virtual size_t queue() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief prepares execution of a handler, has to be called before execute
  //////////////////////////////////////////////////////////////////////////////

  virtual void prepareExecute();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief executes a handler
  //////////////////////////////////////////////////////////////////////////////

  virtual status_t execute() = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief finalizes execution of a handler, has to be called after execute
  //////////////////////////////////////////////////////////////////////////////

  virtual void finalizeExecute();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief tries to cancel an execution
  //////////////////////////////////////////////////////////////////////////////

  virtual bool cancel();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handles error
  //////////////////////////////////////////////////////////////////////////////

  virtual void handleError(basics::Exception const&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds a response
  //////////////////////////////////////////////////////////////////////////////

  virtual void addResponse(HttpHandler*);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the id of the underlying task
  //////////////////////////////////////////////////////////////////////////////

  uint64_t taskId() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the event loop of the underlying task
  //////////////////////////////////////////////////////////////////////////////

  EventLoop eventLoop() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the id of the underlying task or 0 if dettach
  //////////////////////////////////////////////////////////////////////////////

  void setTaskId(uint64_t id, EventLoop);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief execution cycle including error handling and prepare
  //////////////////////////////////////////////////////////////////////////////

  status_t executeFull();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief register the server object
  //////////////////////////////////////////////////////////////////////////////

  void setServer(HttpHandlerFactory* server);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a pointer to the request
  //////////////////////////////////////////////////////////////////////////////

  HttpRequest const* getRequest() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief steal the pointer to the request
  //////////////////////////////////////////////////////////////////////////////

  HttpRequest* stealRequest();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the response
  //////////////////////////////////////////////////////////////////////////////

  HttpResponse* getResponse() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief steal the response
  //////////////////////////////////////////////////////////////////////////////

  HttpResponse* stealResponse();

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a new HTTP response
  //////////////////////////////////////////////////////////////////////////////

  void createResponse(HttpResponse::HttpResponseCode);

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief handler id
  //////////////////////////////////////////////////////////////////////////////

  uint64_t const _handlerId;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief task id or 0
  //////////////////////////////////////////////////////////////////////////////

  uint64_t _taskId;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief event loop
  //////////////////////////////////////////////////////////////////////////////

  EventLoop _loop;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the request
  //////////////////////////////////////////////////////////////////////////////

  HttpRequest* _request;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the response
  //////////////////////////////////////////////////////////////////////////////

  HttpResponse* _response;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the server
  //////////////////////////////////////////////////////////////////////////////

  HttpHandlerFactory* _server;
};
}
}

#endif
