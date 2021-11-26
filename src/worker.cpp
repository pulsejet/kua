#include "worker.hpp"
#include "store-memory.hpp"

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

  // Make data store
  this->store = std::make_shared<StoreMemory>(bucket.id);

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
  auto reqName = interest.isSigned() ? interest.getName().getPrefix(-1) : interest.getName();

  // Ignore interests from localhost
  if (ndn::Name("localhost").isPrefixOf(reqName)) return;

  NDN_LOG_DEBUG("NEW_REQ : #" << m_bucket.id << " : " << reqName);

  // INSERT command
  if (m_bucketPrefix.isPrefixOf(reqName) && reqName.get(-2).toUri() == "INSERT")
  {
    ndn::Name insertName(reqName.get(-1).blockFromValue());
    insertData(insertName, interest);
    return;
  }

  // FETCH command
  for (const auto& delegation : interest.getForwardingHint())
    if (delegation.name.get(-1).toUri() == "FETCH" && m_bucketPrefix.isPrefixOf(delegation.name))
      return this->fetchData(interest);
}

void
Worker::insertData(const ndn::Name& dataName, const ndn::Interest& request)
{
  // Chain replication
  auto reqName = request.getName().getPrefix(-1);
  bool replicate = true;

  if (request.hasApplicationParameters())
  {
    ndn::Name params(request.getApplicationParameters().blockFromValue());
    if (params.get(0).toUri() == "NO-REPLICATE")
      replicate = false;
  }

  if (replicate)
  {
    std::shared_ptr<int> replicaCount = std::make_shared<int>(0);

    for (const auto& host : m_bucket.confirmedHosts)
    {
      // Interest
      ndn::Interest interest(reqName);
      interest.setCanBePrefix(false);
      interest.setMustBeFresh(true);

      // Prevent loops of replication
      ndn::Name params;
      params.append("NO-REPLICATE");
      interest.setApplicationParameters(params.wireEncode());

      // Forwarding hint
      ndn::Name hint(host.first);
      hint.appendNumber(m_bucket.id);
      interest.setForwardingHint(ndn::DelegationList({{15893, hint }}));

      // Signature
      ndn::security::SigningInfo interestSigningInfo;
      interestSigningInfo.setSignedInterestFormat(ndn::security::SignedInterestFormat::V03);
      m_keyChain.sign(interest, interestSigningInfo);

      // Replicate at all replicas
      m_face.expressInterest(interest, [this, request, replicaCount, dataName] (const auto&, const auto& data) {
        (*replicaCount)++;
        NDN_LOG_TRACE("#" << m_bucket.id << " : INSERT_SUCCESS_REPLICATOR : "
                    << data.getName() << " : REPLICA " << *replicaCount);

        // All replicas done
        if ((*replicaCount) >= NUM_REPLICA)
        {
          NDN_LOG_DEBUG("#" << m_bucket.id << " : ALL_REPLICAS : " << dataName);
          replyInsert(request);
        }
      }, nullptr, nullptr);
    }

    return;
  }

  // Request data
  ndn::Interest interest(dataName);

  interest.setCanBePrefix(false);
  interest.setMustBeFresh(false);
  interest.setInterestLifetime(request.getInterestLifetime());

  m_face.expressInterest(interest, [this, request] (const auto&, const auto& data) {
    if (store->put(data))
      replyInsert(request);
    else
      NDN_LOG_TRACE("#" << m_bucket.id << " : FAILED_STORE_PUT : " << data.getName());
  }, nullptr, nullptr);
}

void
Worker::replyInsert(const ndn::Interest& request)
{
  NDN_LOG_TRACE("#" << m_bucket.id << " : INSERT_SUCCESS_REPLY : " << request);
  ndn::Data response(request.getName());
  response.setFreshnessPeriod(ndn::time::seconds(10));
  m_keyChain.sign(response);
  m_face.put(response);
}

void
Worker::fetchData(const ndn::Interest& request)
{
  auto data = this->store->get(request.getName());
  if (data)
    m_face.put(*data);
}

} // namespace kua