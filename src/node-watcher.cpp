#include "node-watcher.hpp"

namespace kua {

NodeWatcher::NodeWatcher(const ndn::Name& syncPrefix, const ndn::Name& nodePrefix,
                         ndn::Face& face, ndn::KeyChain& keyChain)
  : m_syncPrefix(syncPrefix)
  , m_nodePrefix(nodePrefix)
  , m_face(face)
  , m_scheduler(m_face.getIoService())
  , m_keyChain(keyChain)
  , m_rng(ndn::random::getRandomNumberEngine())
  , m_retxDist(3000 * 0.9, 3000 * 1.1)
{
  m_svs = std::make_unique<ndn::svs::SVSync>(
    m_syncPrefix, m_nodePrefix, m_face, std::bind(&NodeWatcher::updateCallback, this, _1));

  retxHeartbeat();
}

void
NodeWatcher::retxHeartbeat()
{
  // Send heartbeat
  ndn::Name dataName(m_nodePrefix);
  dataName.appendTimestamp();
  m_svs->publishData(dataName.wireEncode(), ndn::time::milliseconds(1000));

  // Schedule next heartbeat
  unsigned int delay = m_retxDist(m_rng);
  m_scheduler.schedule(ndn::time::milliseconds(delay), [this] { retxHeartbeat(); });
}

void
NodeWatcher::updateCallback(std::vector<ndn::svs::MissingDataInfo> missingInfo)
{
  auto now = std::chrono::system_clock::now();
  for (auto m : missingInfo)
  {
    m_nodeMap[m.nodeId] = now;
  }
}

std::vector<ndn::Name>
NodeWatcher::getNodeList()
{
  std::vector<ndn::Name> list;
  auto now = std::chrono::system_clock::now();

  for (auto const& [name, time] : m_nodeMap)
  {
    double duration = std::chrono::duration<double, std::milli>(now - time).count();
    if (duration < EXCLUDE_TIME_MS)
    {
      list.push_back(name);
    }
  }

  return list;
}

} // namespace kua