#pragma once

#include <ndn-svs/svsync.hpp>

#include "config-bundle.hpp"
#include "node-watcher.hpp"

namespace kua {

class Bidder
{
public:
  /** Initialize the bidder with the sync prefix */
  Bidder(ConfigBundle& configBundle, NodeWatcher& nodeWatcher);

private:
  /**
   * Initialize the bidder
   */
  void initialize();

  /** On SVS update */
  void updateCallback(const std::vector<ndn::svs::MissingDataInfo>& missingInfo);

  /** Process packet from master */
  void processMasterMessage(const ndn::Data& data);

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