#pragma once

#include <ndn-cxx/util/scheduler.hpp>

#include "config-bundle.hpp"
#include "bucket.hpp"
#include "store.hpp"

namespace kua {

class Worker
{
public:
  Worker(ConfigBundle& configBundle, const Bucket& bucket);

  ~Worker();

private:
  void
  onRegisterFailed(const ndn::Name& prefix, const std::string& reason);

  void
  onInterest(const ndn::InterestFilter&, const ndn::Interest& interest);

  void
  run();

  void
  insert(const ndn::Name& dataName, const ndn::Interest& request);

  void
  insertNoReplicate(const ndn::Name& dataName, const ndn::Interest& request);

  void
  replyInsert(const ndn::Interest& request);

  void
  fetch(const ndn::Interest& request);

public:
  std::shared_ptr<Store> store;

private:
  ConfigBundle& m_configBundle;
  const Bucket& m_bucket;

  ndn::Name m_nodePrefix;
  ndn::Face m_face;
  ndn::Scheduler m_scheduler;
  ndn::KeyChain& m_keyChain;

  ndn::Name m_bucketPrefix;

  size_t m_failedRegistrations = 0;
};

} // namespace kua