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

  /** On SVS update */
  void updateCallback(const std::vector<ndn::svs::MissingDataInfo>& missingInfo);

  /** Start a new auction */
  void auction();

private:
  ConfigBundle& m_configBundle;
  ndn::Name m_syncPrefix;
  ndn::Name m_nodePrefix;
  ndn::Face& m_face;
  ndn::Scheduler m_scheduler;
  ndn::KeyChain& m_keyChain;
  NodeWatcher& m_nodeWatcher;

  ndn::random::RandomNumberEngine& m_rng;

  bool m_initialized = false;

  std::unique_ptr<ndn::svs::SVSync> m_svs;
};

} // namespace kua