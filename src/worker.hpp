#pragma once

#include <ndn-cxx/util/scheduler.hpp>

#include "config-bundle.hpp"
#include "bucket.hpp"

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