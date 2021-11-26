#include "worker.hpp"
#include "store-memory.hpp"
#include "command-codes.hpp"

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

  if (m_failedRegistrations >= 50) {
    NDN_LOG_ERROR("FATAL: Too many failures to register prefix '" << prefix
             << "' with the local forwarder (" << reason << ")");
    m_face.shutdown();
    return;
  }

  m_scheduler.schedule(ndn::time::milliseconds(300), [this, prefix] {
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

  // Command Code
  if (reqName.size() > 1 && reqName[-1].isNumber() &&
      (m_bucketPrefix.isPrefixOf(reqName) || m_nodePrefix.isPrefixOf(reqName)))
  {
    uint64_t ccode = reqName[-1].toNumber();

    if (ccode & CommandCodes::INSERT)
    {
      ndn::Name insertName(reqName.get(-2).blockFromValue());
      insert(insertName, interest, ccode);
      return;
    }
  }

  // FETCH command
  for (const auto& delegation : interest.getForwardingHint())
    if (delegation.name.size() > 1 && delegation.name[-1].isNumber() &&
        delegation.name[-1].toNumber() == CommandCodes::FETCH)
      return this->fetch(interest);
}

void
Worker::insert(const ndn::Name& dataName, const ndn::Interest& request, const uint64_t& commandCode)
{
  if (commandCode & CommandCodes::NO_REPLICATE)
    return insertNoReplicate(dataName, request, commandCode);

  std::shared_ptr<int> replicaCount = std::make_shared<int>(0);

  for (const auto& host : m_bucket.confirmedHosts)
  {
    // Interest
    ndn::Name interestName(host.first);
    interestName.appendNumber(m_bucket.id);
    interestName.append(dataName.wireEncode());
    interestName.appendNumber(commandCode | CommandCodes::NO_REPLICATE);

    ndn::Interest interest(interestName);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(true);

    // Signature
    ndn::security::SigningInfo interestSigningInfo;
    interestSigningInfo.setSha256Signing();
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
}

void
Worker::insertNoReplicate(const ndn::Name& dataName, const ndn::Interest& request, const uint64_t& commandCode)
{
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
  ndn::security::SigningInfo info;
  info.setSha256Signing();
  m_keyChain.sign(response, info);
  m_face.put(response);
}

void
Worker::fetch(const ndn::Interest& request)
{
  auto data = this->store->get(request.getName());
  if (data)
    m_face.put(*data);
}

} // namespace kua