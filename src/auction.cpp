#include "auction.hpp"

#include <ndn-cxx/encoding/encoder.hpp>
#include <ndn-svs/version-vector.hpp>

namespace kua {

AuctionMessage::AuctionMessage(Type _messageType,
                               auction_id_t _auctionId,
                               bucket_id_t _bucketId)
  : messageType(_messageType)
  , auctionId(_auctionId)
  , bucketId(_bucketId)
{}

ndn::Block
AuctionMessage::wireEncode() const
{
  ndn::encoding::Encoder enc;

  size_t totalLength = 0;

#define K_ENCODE_NNI(VAR_V, TLV_V) {\
    size_t valLength = enc.prependNonNegativeInteger(VAR_V); \
    totalLength += enc.prependVarNumber(valLength); \
    totalLength += enc.prependVarNumber(TLV_V); \
    totalLength += valLength; \
  }

#define K_ENCODE_BLK(VAR_V, TLV_V) {\
    size_t valLength = enc.prependBlock(VAR_V); \
    totalLength += enc.prependVarNumber(valLength); \
    totalLength += enc.prependVarNumber(TLV_V); \
    totalLength += valLength; \
  }

  if (messageType == Type::Bid)
    K_ENCODE_NNI(bidAmount, tlv::AuctionBidAmount);

  if (!winner.empty())
    K_ENCODE_BLK(winner.wireEncode(), tlv::AuctionWinner);

  if (!winnerList.empty())
  {
    ndn::svs::VersionVector vv;
    for (const auto& w : winnerList)
      vv.set(w, 1);
    K_ENCODE_BLK(vv.encode(), tlv::AuctionWinnerList);
  }

  K_ENCODE_NNI(bucketId, tlv::BucketId);
  K_ENCODE_NNI(auctionId, tlv::AuctionId);
  K_ENCODE_NNI(messageType, tlv::AuctionMessageType);

  totalLength += enc.prependVarNumber(totalLength);
  totalLength += enc.prependVarNumber(kua::tlv::AuctionMessage);

  return enc.block();

#undef K_ENCODE_NNI
}

void
AuctionMessage::wireDecode(const ndn::Block& block)
{
  block.parse();

  if (block.type() != tlv::AuctionMessage)
    NDN_THROW(ndn::tlv::Error("Expected AuctionMessage"));

#define K_READ_NNI(VAR_V, TLV_V) { \
    VAR_V = ndn::encoding::readNonNegativeInteger(block.get(TLV_V)); \
  }

  try {
    messageType = (Type) (ndn::encoding::readNonNegativeInteger(block.get(tlv::AuctionMessageType)));
    K_READ_NNI(auctionId, tlv::AuctionId);
    K_READ_NNI(bucketId, tlv::BucketId);
  } catch (const std::exception& ex) {
    NDN_THROW(ndn::tlv::Error("Invalid AuctionMessage"));
  }

  if (block.find(tlv::AuctionBidAmount) != block.elements_end())
    K_READ_NNI(bidAmount, tlv::AuctionBidAmount);

  if (block.find(tlv::AuctionWinner) != block.elements_end())
    winner = ndn::Name(block.get(tlv::AuctionWinner).blockFromValue());

  if (block.find(tlv::AuctionWinnerList) != block.elements_end())
  {
    ndn::svs::VersionVector vv(block.get(tlv::AuctionWinnerList).blockFromValue());
    winnerList.clear();
    for (const auto& w : vv)
      winnerList.push_back(w.first);
  }

#undef K_READ_NNI
}

} // namespace kua