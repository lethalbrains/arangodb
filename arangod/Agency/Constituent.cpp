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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterComm.h"
#include "Logger/Logger.h"
#include "Basics/ConditionLocker.h"

#include "Constituent.h"
#include "Agent.h"

#include <velocypack/Iterator.h>    
#include <velocypack/velocypack-aliases.h> 

#include <chrono>
#include <thread>

using namespace arangodb::consensus;
using namespace arangodb::rest;
using namespace arangodb::velocypack;

// Configure with agent's configuration
void Constituent::configure(Agent* agent) {

  _agent = agent;

  if (size() == 1) {
    _role = LEADER;
  } else {
    try {
      _votes.resize(size());
    } catch (std::exception const& e) {
      LOG_TOPIC(ERR, Logger::AGENCY) <<
        "Cannot resize votes vector to " << size();
      LOG_TOPIC(ERR, Logger::AGENCY) << e.what();
    }
    
    _id = _agent->config().id;

    if (_agent->config().notify) {// (notify everyone) 
      notifyAll();
    }
  }
  
}

// Default ctor
Constituent::Constituent() :
  Thread("Constituent"), _term(0), _leader_id(0), _id(0), _gen(std::random_device()()),
  _role(FOLLOWER), _agent(0) {}

// Shutdown if not already
Constituent::~Constituent() {
  shutdown();
}

// Random sleep times in election process
duration_t Constituent::sleepFor (double min_t, double max_t) {
  dist_t dis(min_t, max_t);
  return duration_t((long)std::round(dis(_gen)*1000.0));
}

// Get my term
term_t Constituent::term() const {
  return _term;
}

// Update my term
void Constituent::term(term_t t) {

  if (_term != t) {

    LOG_TOPIC(INFO, Logger::AGENCY) << "Updating term to "
                                    << t << "and persisting";

    static std::string const path = "/_api/document?collection=election";
    std::map<std::string, std::string> headerFields;
    
    Builder body;
    body.add(VPackValue(VPackValueType::Object));
    std::ostringstream i_str;
    i_str << std::setw(20) << std::setfill('0') << index;
    body.add("term", Value(term));
    body.add("voted_for", Value((uint32_t)_voted_for));
    body.close();
    
    std::unique_ptr<arangodb::ClusterCommResult> res =
      arangodb::ClusterComm::instance()->syncRequest(
        "1", 1, _agent->config().end_point, GeneralRequest::RequestType::POST,
        path, body.toJson(), headerFields, 0.0);
    
    if (res->status != CL_COMM_SENT) {
      LOG_TOPIC(ERR, Logger::AGENCY) << res->status << ": " << CL_COMM_SENT
                                     << ", " << res->errorMessage;
      LOG_TOPIC(ERR, Logger::AGENCY)
        << res->result->getBodyVelocyPack()->toJson();
    }
    
  }
  
}


role_t Constituent::role () const {
  return _role;
}

void Constituent::follow (term_t t) {
  if (_role != FOLLOWER) {
    LOG_TOPIC(INFO, Logger::AGENCY) << "Role change: Converted to follower in term " << t;
  }
  this->term(t);
  _votes.assign(_votes.size(),false); // void all votes
  _role = FOLLOWER;
}

void Constituent::lead () {
  if (_role != LEADER) {
    LOG_TOPIC(INFO, Logger::AGENCY) << "Role change: Converted to leader in term " << _term ;
    _agent->lead(); // We need to rebuild spear_head and read_db;
  }
  _role = LEADER;
  _leader_id = _id;
}

void Constituent::candidate () {
  if (_role != CANDIDATE)
    LOG_TOPIC(INFO, Logger::AGENCY) << "Role change: Converted to candidate in term " << _term ;
  _role = CANDIDATE;
}

bool Constituent::leading () const {
  return _role == LEADER;
}

bool Constituent::following () const {
  return _role == FOLLOWER;
}

bool Constituent::running () const {
  return _role == CANDIDATE;
}

id_t Constituent::leaderID ()  const {
  return _leader_id;
}

size_t Constituent::size() const {
  return _agent->config().size();
}

std::string const& Constituent::end_point(id_t id) const {
  return _agent->config().end_points[id];
}

std::vector<std::string> const& Constituent::end_points() const {
  return _agent->config().end_points;
}

size_t Constituent::notifyAll () {

  // Last process notifies everyone 
  std::stringstream path;
  
  path << "/_api/agency_priv/notifyAll?term=" << _term << "&agencyId=" << _id;

  // Body contains endpoints list
  Builder body;
  body.openObject();
  body.add("endpoints", VPackValue(VPackValueType::Array));
  for (auto const& i : end_points()) {
    body.add(Value(i));
  }
  body.close();
  body.close();
  
  // Send request to all but myself
  for (id_t i = 0; i < size(); ++i) {
    if (i != _id) {
      std::unique_ptr<std::map<std::string, std::string>> headerFields =
        std::make_unique<std::map<std::string, std::string> >();
      arangodb::ClusterComm::instance()->asyncRequest(
        "1", 1, end_point(i), GeneralRequest::RequestType::POST, path.str(),
        std::make_shared<std::string>(body.toString()), headerFields, nullptr,
        0.0, true);
    }
  }
  
  return size()-1;
}

bool Constituent::vote (
  term_t term, id_t id, index_t prevLogIndex, term_t prevLogTerm) {
  if (term > _term || (_term==term&&_leader_id==id)) {
    this->term(term);
    _cast = true;      // Note that I voted this time around.
    _voted_for = id;   // The guy I voted for I assume leader.
    _leader_id = id;
    if (_role>FOLLOWER)
      follow (_term);
    _agent->persist(_term,_voted_for);
    _cv.signal();
    return true;
  } else {             // Myself running or leading
    return false;
  }
}

void Constituent::update (term_t t, id_t i) {
  
}

void Constituent::gossip (const constituency_t& constituency) {
  // TODO: Replace lame notification by gossip protocol
}

const constituency_t& Constituent::gossip () {
  // TODO: Replace lame notification by gossip protocol
  return _constituency;
}

void Constituent::callElection() {

  try {
    _votes.at(_id) = true; // vote for myself
  } catch (std::out_of_range const& e) {
    LOG_TOPIC(ERR, Logger::AGENCY) << "_votes vector is not properly sized!";
    LOG_TOPIC(ERR, Logger::AGENCY) << e.what();
  }
  _cast = true;
  if(_role == CANDIDATE)
    this->term(_term+1);            // raise my term
  
  std::string body;
  std::vector<ClusterCommResult> results(_agent->config().end_points.size());
  std::stringstream path;
  
  path << "/_api/agency_priv/requestVote?term=" << _term << "&candidateId=" << _id
       << "&prevLogIndex=" << _agent->lastLog().index << "&prevLogTerm="
       << _agent->lastLog().term;

  for (id_t i = 0; i < _agent->config().end_points.size(); ++i) { // Ask everyone for their vote
    if (i != _id && end_point(i) != "") {
      std::unique_ptr<std::map<std::string, std::string>> headerFields =
        std::make_unique<std::map<std::string, std::string> >();
      results[i] = arangodb::ClusterComm::instance()->asyncRequest(
        "1", 1, _agent->config().end_points[i], GeneralRequest::RequestType::GET,
        path.str(), std::make_shared<std::string>(body), headerFields, nullptr,
        _agent->config().min_ping, true);
    }
  }
  
  std::this_thread::sleep_for(sleepFor(.5*_agent->config().min_ping, .8*_agent->config().min_ping)); // Wait timeout
  
  for (id_t i = 0; i < _agent->config().end_points.size(); ++i) { // Collect votes
    if (i != _id && end_point(i) != "") {
      ClusterCommResult res = arangodb::ClusterComm::instance()->
        enquire(results[i].operationID);
      
      if (res.status == CL_COMM_SENT) { // Request successfully sent 
        res = arangodb::ClusterComm::instance()->wait("1", 1, results[i].operationID, "1");
        std::shared_ptr<Builder > body = res.result->getBodyVelocyPack();
        if (body->isEmpty()) {
          continue;
        } else {
          if (body->slice().isArray() || body->slice().isObject()) {
            for (auto const& it : VPackObjectIterator(body->slice())) {
              std::string const key(it.key.copyString());
              if (key == "term") {
                if (it.value.isUInt()) {
                  if (it.value.getUInt() > _term) { // follow?
                    follow(it.value.getUInt());
                    break;
                  }
                }
              } else if (key == "voteGranted") {
                if (it.value.isBool()) {
                  _votes[i] = it.value.getBool();
                }
              }
            }
          }
        }
      } else { // Request failed
        _votes[i] = false;
      }
    }
  }
  
  size_t yea = 0;
  for (size_t i = 0; i < size(); ++i) {
    if (_votes[i]){
      yea++;
    }    
  }
  if (yea > size()/2){
    lead();
  } else {
    follow(_term);
  }
}

void Constituent::beginShutdown() {
  Thread::beginShutdown();
}

#include <iostream>
void Constituent::run() {
  
  // Always start off as follower
  while (!this->isStopping() && size() > 1) { 
    if (_role == FOLLOWER) {
      _cast = false;                           // New round set not cast vote
      std::this_thread::sleep_for(             // Sleep for random time
        sleepFor(_agent->config().min_ping, _agent->config().max_ping)); 
      if (!_cast) {
        candidate();                           // Next round, we are running
      }
    } else {
      callElection();                          // Run for office
    }
  }

}


