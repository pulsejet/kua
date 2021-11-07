#pragma once

#include <ndn-svs/svsync.hpp>

#include "config-bundle.hpp"
#include "node-watcher.hpp"

namespace kua {

class Master
{
public:
  /** Initialize the bidder with the sync prefix */
  Master(ConfigBundle& configBundle, NodeWatcher& nodeWatcher);

private:
  /**
   * Attempt to initialize master
   * If there are less than 3 nodes known, init will retry later
   */
  void initialize();

private:
  ConfigBundle& m_configBundle;
  ndn::Name m_syncPrefix;
  ndn::Name m_nodePrefix;
  ndn::Face& m_face;
  ndn::Scheduler m_scheduler;
  ndn::KeyChain& m_keyChain;
  NodeWatcher& m_nodeWatcher;

  ndn::random::RandomNumberEngine& m_rng;

  std::unique_ptr<ndn::svs::SVSync> m_svs;
};

} // namespace kua