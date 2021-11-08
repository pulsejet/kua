#include "worker.hpp"

#include <ndn-cxx/util/logger.hpp>

namespace kua {

NDN_LOG_INIT(kua.worker);

Worker::Worker(ConfigBundle& configBundle, const Bucket& bucket)
  : m_configBundle(configBundle)
  , m_bucket(bucket)
  , m_nodePrefix(configBundle.nodePrefix)
  , m_face(configBundle.face)
  , m_scheduler(m_face.getIoService())
  , m_keyChain(configBundle.keyChain)
{
  NDN_LOG_INFO("Constructing worker for #" << bucket.id);
}

} // namespace kua