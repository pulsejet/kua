#pragma once

#include <ndn-cxx/util/scheduler.hpp>

#include "config-bundle.hpp"
#include "bucket.hpp"

namespace kua {

class Worker
{
public:
  Worker(ConfigBundle& configBundle, const Bucket& bucket);

private:
  ConfigBundle& m_configBundle;
  const Bucket& m_bucket;

  ndn::Name m_nodePrefix;
  ndn::Face& m_face;
  ndn::Scheduler m_scheduler;
  ndn::KeyChain& m_keyChain;
};

} // namespace kua