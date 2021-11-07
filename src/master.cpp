#include "master.hpp"

#include <ndn-cxx/util/logger.hpp>

namespace kua {

NDN_LOG_INIT(kua.master);

Master::Master(ConfigBundle& configBundle, NodeWatcher& nodeWatcher)
  : m_configBundle(configBundle)
  , m_syncPrefix(ndn::Name(configBundle.kuaPrefix).append("sync").append("auction"))
  , m_nodePrefix(configBundle.nodePrefix)
  , m_face(configBundle.face)
  , m_scheduler(m_face.getIoService())
  , m_keyChain(configBundle.keyChain)
  , m_nodeWatcher(nodeWatcher)
  , m_rng(ndn::random::getRandomNumberEngine())
{
  NDN_LOG_INFO("Constructing Master");
  initialize();
}

void
Master::initialize()
{
  auto nodeList = m_nodeWatcher.getNodeList();

  if (nodeList.size() < 3)
  {
    NDN_LOG_TRACE("Will not initialize Master without 3 nodes known");
    m_scheduler.schedule(ndn::time::milliseconds(1000), [this] { initialize(); });
    return;
  }

  NDN_LOG_DEBUG("Initializing Master");
}

} // namespace kua