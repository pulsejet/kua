#include "node-watcher.hpp"

#include <ndn-cxx/util/logger.hpp>

namespace kua {

NDN_LOG_INIT(kua.nodewatcher);

NodeWatcher::NodeWatcher(ConfigBundle& configBundle)
  : m_syncPrefix(ndn::Name(configBundle.kuaPrefix).append("sync").append("health"))
  , m_nodePrefix(configBundle.nodePrefix)
  , m_face(configBundle.face)
  , m_scheduler(m_face.getIoService())
  , m_keyChain(configBundle.keyChain)
  , m_rng(ndn::random::getRandomNumberEngine())
  , m_retxDist(3000 * 0.9, 3000 * 1.1)
{
  NDN_LOG_INFO("Constructing NodeWatcher");

  m_svs = std::make_unique<ndn::svs::SVSync>(
    m_syncPrefix, m_nodePrefix, m_face, std::bind(&NodeWatcher::updateCallback, this, _1));

  retxHeartbeat();
}

void
NodeWatcher::retxHeartbeat()
{
  NDN_LOG_TRACE("retxHeartbeat");

  // Send heartbeat
  ndn::Name dataName(m_nodePrefix);
  dataName.appendTimestamp();
  m_svs->publishData(dataName.wireEncode(), ndn::time::milliseconds(1000));

  // Schedule next heartbeat
  unsigned int delay = m_retxDist(m_rng);
  m_scheduler.schedule(ndn::time::milliseconds(delay), [this] { retxHeartbeat(); });
}

void
NodeWatcher::updateCallback(const std::vector<ndn::svs::MissingDataInfo>& missingInfo)
{
  auto now = std::chrono::system_clock::now();
  for (auto m : missingInfo)
  {
    NDN_LOG_TRACE("update " << m.nodeId);
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