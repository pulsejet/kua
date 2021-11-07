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

  // Initialize bucket list
  for (unsigned int i = 0; i < NUM_BUCKETS; i++)
  {
    Bucket b;
    b.id = i;
    m_buckets.push_back(b);
  }

  // Initialize SVS
  m_svs = std::make_unique<ndn::svs::SVSync>(
    m_syncPrefix, MASTER_PREFIX, m_face, std::bind(&Master::updateCallback, this, _1));

  // Wait 3s before initialization
  m_scheduler.schedule(ndn::time::milliseconds(3000), [this] { initialize(); });
}

void
Master::initialize()
{
  auto nodeList = m_nodeWatcher.getNodeList();

  if (nodeList.size() < 2)
  {
    NDN_LOG_TRACE("Will not initialize Master without 3 nodes known");
    m_scheduler.schedule(ndn::time::milliseconds(1000), [this] { initialize(); });
    return;
  }

  NDN_LOG_DEBUG("Initializing Master");

  m_initialized = true;

  auction();
}

void
Master::auction()
{
  if (!currentAuctionId) {
    for (unsigned int i = 0; i < m_buckets.size(); i++)
    {
      Bucket& b = m_buckets[i];
      if (b.confirmedHosts.size() == 0)
      {
        auction(i);
        return;
      }
    }
  }

  auctionRecheckEvent = m_scheduler.schedule(ndn::time::milliseconds(1000), [this] { auction(); });
}

void
Master::auction(unsigned int id)
{
  currentAuctionId = m_rng();
  NDN_LOG_INFO("Starting auction for #" << id << " AID " << currentAuctionId);

  ndn::Name auctionInfo;
  auctionInfo.append("AUCTION");
  auctionInfo.appendNumber(id);
  auctionInfo.appendNumber(currentAuctionId);

  m_svs->publishData(auctionInfo.wireEncode(), ndn::time::milliseconds(1000));
}

void
Master::updateCallback(const std::vector<ndn::svs::MissingDataInfo>& missingInfo)
{
  if (!m_initialized) return;

  for (const auto m : missingInfo) {
    for (ndn::svs::SeqNo i = m.low; i <= m.high; i++) {
      m_svs->fetchData(m.nodeId, i, std::bind(&Master::processMessage, this, _1));
    }
  }
}

void
Master::processMessage(const ndn::Data& data)
{
  ndn::Name msg(data.getContent().blockFromValue());
  NDN_LOG_TRACE("RECV_MSG=" << msg);
  auto type = msg.get(0).toUri();

  if (type == "BID") {
    unsigned int bucketId = msg.get(1).toNumber();
    unsigned int auctionId = msg.get(2).toNumber();
    unsigned int bidAmount = msg.get(3).toNumber();
    NDN_LOG_DEBUG("RECV_BID for #" << bucketId << " AID " << auctionId << " $" << bidAmount);
  }
}

} // namespace kua