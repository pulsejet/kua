#include "nlsr.hpp"
#include <ndn-cxx/util/logger.hpp>

namespace kua {

NDN_LOG_INIT(kua.nlsr);

const ndn::Name LOCALHOST_PREFIX = ndn::Name("/localhost/nlsr");
const ndn::Name LSDB_PREFIX = ndn::Name(LOCALHOST_PREFIX).append("lsdb");
const ndn::Name NAME_UPDATE_PREFIX = ndn::Name(LOCALHOST_PREFIX).append("prefix-update");

const ndn::Name RT_PREFIX = ndn::Name(LOCALHOST_PREFIX).append("routing-table");

const uint32_t ERROR_CODE_TIMEOUT = 10060;
const uint32_t RESPONSE_CODE_EXISTS = 406;
const uint32_t RESPONSE_CODE_SUCCESS = 200;
const uint32_t RESPONSE_CODE_SAVE_OR_DELETE = 205;

NLSR::NLSR(ndn::security::KeyChain& keyChain, ndn::Face& face)
  : m_rng(ndn::random::getRandomNumberEngine())
  , m_face(face)
  , m_keyChain(keyChain)
  , m_scheduler(face.getIoService())
  , m_jitter(1, 100)
{}

void
NLSR::advertise(const ndn::Name& prefix)
{
  m_face.expressInterest(advertiseInterest(prefix), [this, prefix]
      (const auto&, const auto& data)
    {
      if (data.getMetaInfo().getType() == ndn::tlv::ContentType_Nack) {
        NDN_LOG_DEBUG("ERROR: Run-time advertise/withdraw disabled");
        return;
      }

      ndn::nfd::ControlResponse response;

      try {
        response.wireDecode(data.getContent().blockFromValue());
      }
      catch (const std::exception& e) {
        NDN_LOG_DEBUG("ERROR: Control response decoding error");
        return;
      }

      uint32_t code = response.getCode();

      if (code != RESPONSE_CODE_SUCCESS && code != RESPONSE_CODE_SAVE_OR_DELETE && code != RESPONSE_CODE_EXISTS) {

        NDN_LOG_DEBUG(response.getText());
        NDN_LOG_DEBUG("Name prefix update error (code: " << code << ")");

        m_scheduler.schedule(ndn::time::milliseconds(500 + m_jitter(m_rng)), [this, prefix] () {
          advertise(prefix);
        });
        return;
      }

      NDN_LOG_DEBUG("Announced " << prefix << " to NLSR!");
  }, nullptr, nullptr);
}

ndn::Interest
NLSR::advertiseInterest(const ndn::Name& name)
{
  ndn::Name::Component verb("advertise");
  return getNamePrefixInterst(name, verb);
}

ndn::Interest
NLSR::getNamePrefixInterst(const ndn::Name& name,
                           const ndn::Name::Component& verb)
{
  ndn::nfd::ControlParameters parameters;
  parameters.setName(name);
  parameters.setFlags(1);

  ndn::Name commandName = NAME_UPDATE_PREFIX;
  commandName.append(verb);
  commandName.append(parameters.wireEncode());

  ndn::security::InterestSigner signer(m_keyChain);
  auto commandInterest = signer.makeCommandInterest(commandName,
                           ndn::security::signingByIdentity(m_keyChain.getPib().getDefaultIdentity()));
  commandInterest.setMustBeFresh(true);

  return commandInterest;
}

} // namespace kua