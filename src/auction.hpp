#pragma once

#include "tlv.hpp"
#include <ndn-cxx/encoding/block.hpp>
#include <ndn-cxx/name.hpp>

typedef unsigned int auction_id_t;
typedef unsigned int bucket_id_t;

namespace kua {

struct AuctionMessage
{
  enum {
    TypeAuction = 1,
    TypeBid = 2,
    TypeWin = 3,
    TypeWinAck = 4,
    TypeAuctionEnd = 5,
  };

  unsigned int messageType;

  auction_id_t auctionId;
  bucket_id_t bucketId;

  AuctionMessage() = default;

  AuctionMessage(unsigned int _messageType,
                 auction_id_t _auctionId,
                 bucket_id_t _bucketId);

  // TypeBid
  unsigned int bidAmount;

  // TypeWin and TypeWinAck
  ndn::Name winner;

  // TypeAuctionEnd
  std::vector<ndn::Name> winnerList;

  void
  wireDecode(const ndn::Block& block);

  ndn::Block
  wireEncode() const;
};

} // namespace kua