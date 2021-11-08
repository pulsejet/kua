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

  if (nodeList.size() < NUM_REPLICA)
  {
    NDN_LOG_TRACE("Will not initialize Master without " << NUM_REPLICA << " nodes known");
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
  if (!m_currentAuctionId) {
    for (unsigned int i = 0; i < m_buckets.size(); i++)
    {
      Bucket& b = m_buckets[i];
      if (b.confirmedHosts.size() == 0)
      {
        auction(i);
        break;
      }
    }
  }

  m_auctionRecheckEvent = m_scheduler.schedule(ndn::time::milliseconds(1000), [this] { auction(); });
}

void
Master::auction(unsigned int id)
{
  m_currentAuctionId = m_rng();
  m_currentAuctionBucketId = id;
  NDN_LOG_INFO("Starting auction for #" << m_currentAuctionBucketId << " AID " << m_currentAuctionId);

  m_currentAuctionBids.clear();
  m_currentAuctionNumBidsExpected = m_nodeWatcher.getNodeList().size();
  m_buckets[m_currentAuctionBucketId].pendingHosts.clear();

  ndn::Name auctionInfo;
  auctionInfo.append("AUCTION");
  auctionInfo.appendNumber(id);
  auctionInfo.appendNumber(m_currentAuctionId);

  m_svs->publishData(auctionInfo.wireEncode(), ndn::time::milliseconds(1000));
}

void
Master::updateCallback(const std::vector<ndn::svs::MissingDataInfo>& missingInfo)
{
  if (!m_initialized) return;

  for (const auto m : missingInfo) {
    for (ndn::svs::SeqNo i = m.low; i <= m.high; i++) {
      m_svs->fetchData(m.nodeId, i, std::bind(&Master::processMessage, this, m.nodeId, _1), 10);
    }
  }
}

void
Master::processMessage(const ndn::Name& sender, const ndn::Data& data)
{
  ndn::Name msg(data.getContent().blockFromValue());
  NDN_LOG_TRACE("RECV_MSG=" << msg);
  auto type = msg.get(0).toUri();

  if (type == "BID")
  {
    unsigned int bucketId = msg.get(1).toNumber();
    unsigned int auctionId = msg.get(2).toNumber();
    unsigned int bidAmount = msg.get(3).toNumber();

    if (auctionId != m_currentAuctionId)
    {
      // Unknown auction
      return;
    }

    for (const Bid& bid : m_currentAuctionBids)
    {
      if (bid.bidder == sender)
      {
        // Duplicate bid
        return;
      }
    }

    Bid bid { sender, bidAmount };
    m_currentAuctionBids.push_back(bid);
    NDN_LOG_DEBUG("RECV_BID from " << sender << " for #" << bucketId << " AID " << auctionId << " $" << bidAmount);

    if (m_currentAuctionBids.size() == m_currentAuctionNumBidsExpected)
    {
      declareAuctionWinners();
    }
  }
  else if (type == "WIN_ACK")
  {
    unsigned int bucketId = msg.get(1).toNumber();
    unsigned int auctionId = msg.get(2).toNumber();

    if (auctionId != m_currentAuctionId || bucketId != m_currentAuctionBucketId)
    {
      // Unknown auction
      return;
    }

    // Add to confirmed hosts if pending
    auto& m = m_buckets[m_currentAuctionBucketId].pendingHosts;
    if (m.count(sender))
    {
      m.erase(sender);
      m_buckets[m_currentAuctionBucketId].confirmedHosts[sender] = 1;
    }

    // Check if all pending are confirmed now
    if (m_buckets[m_currentAuctionBucketId].pendingHosts.size() == 0)
      endAuction();
  }
}

void
Master::declareAuctionWinners()
{
  std::sort(m_currentAuctionBids.begin(), m_currentAuctionBids.end());

  for (int i = m_currentAuctionBids.size() - 1; i >= 0; i--)
  {
    const Bid& bid = m_currentAuctionBids[i];

    NDN_LOG_INFO(bid.bidder << " won #" << m_currentAuctionBucketId << " for " << bid.amount);

    m_buckets[m_currentAuctionBucketId].pendingHosts[bid.bidder] = 1;

    // Inform the winner
    ndn::Name winInfo;
    winInfo.append("WIN");
    winInfo.appendNumber(m_currentAuctionBucketId);
    winInfo.appendNumber(m_currentAuctionId);
    winInfo.append(bid.bidder.wireEncode());
    m_svs->publishData(winInfo.wireEncode(), ndn::time::milliseconds(1000));

    if (m_buckets[m_currentAuctionBucketId].pendingHosts.size() >= NUM_REPLICA)
    {
      break;
    }
  }

  if (m_buckets[m_currentAuctionBucketId].pendingHosts.size() == 0)
    endAuction();
}

void
Master::endAuction()
{
  ndn::svs::VersionVector winnerVector;
  for (const auto& n : m_buckets[m_currentAuctionBucketId].confirmedHosts)
  {
    winnerVector.set(n.first, 1);
  }

  ndn::Name auctionInfo;
  auctionInfo.append("AUCTION_END");
  auctionInfo.appendNumber(m_currentAuctionBucketId);
  auctionInfo.appendNumber(m_currentAuctionId);
  auctionInfo.append(winnerVector.encode());
  m_svs->publishData(auctionInfo.wireEncode(), ndn::time::milliseconds(1000));

  m_currentAuctionId = 0;
  auction();
}

} // namespace kua