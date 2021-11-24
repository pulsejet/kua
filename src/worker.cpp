#include "worker.hpp"

#include <ndn-cxx/util/logger.hpp>
#include <thread>

namespace kua {

NDN_LOG_INIT(kua.worker);

Worker::Worker(ConfigBundle& configBundle, const Bucket& bucket)
  : m_configBundle(configBundle)
  , m_bucket(bucket)
  , m_nodePrefix(configBundle.nodePrefix)
  , m_scheduler(m_face.getIoService())
  , m_keyChain(configBundle.keyChain)
  , m_bucketPrefix(ndn::Name(configBundle.kuaPrefix).appendNumber(bucket.id))
{
  NDN_LOG_INFO("Constructing worker for #" << bucket.id << " " << m_nodePrefix);

  // Get all interests
  m_face.setInterestFilter("/", std::bind(&Worker::onInterest, this, _1, _2));

  // Register for unique node
  m_face.registerPrefix(ndn::Name(m_nodePrefix).appendNumber(bucket.id),
                        nullptr, // RegisterPrefixSuccessCallback is optional
                        std::bind(&Worker::onRegisterFailed, this, _1, _2));

  // Register for bucket
  m_face.registerPrefix(m_bucketPrefix,
                        nullptr, // RegisterPrefixSuccessCallback is optional
                        std::bind(&Worker::onRegisterFailed, this, _1, _2));

  std::thread thread(std::bind(&Worker::run, this));
  thread.detach();
}

Worker::~Worker() {
  m_face.shutdown();
}

void
Worker::run()
{
  m_face.processEvents();
}

void
Worker::onRegisterFailed(const ndn::Name& prefix, const std::string& reason)
{
  NDN_LOG_ERROR("ERROR: Failed to register prefix '" << prefix
             << "' with the local forwarder (" << reason << ")");

  if (m_failedRegistrations >= 5) {
    m_face.shutdown();
    return;
  }

  m_scheduler.schedule(ndn::time::milliseconds(30), [this, prefix] {
    m_face.registerPrefix(prefix,
                          nullptr, // RegisterPrefixSuccessCallback is optional
                          std::bind(&Worker::onRegisterFailed, this, _1, _2));
  });

  m_failedRegistrations += 1;
}

void
Worker::onInterest(const ndn::InterestFilter&, const ndn::Interest& interest)
{
  // Ignore interests from localhost
  if (ndn::Name("localhost").isPrefixOf(interest.getName())) return;

  NDN_LOG_INFO("Interest " << interest);
}

} // namespace kua