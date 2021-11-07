#include <iostream>

#include "node-watcher.hpp"

int
main(int argc, char *argv[])
{
  if (argc < 3)
  {
    std::cerr << "Usage: kua <kua-prefix> <node-prefix>" << std::endl;
    abort();
  }

  // Get arguments
  const ndn::Name kuaPrefix(argv[1]);
  const ndn::Name nodePrefix(argv[2]);

  // Start face and keychain
  ndn::Face face;
  ndn::KeyChain keyChain;

  // Start watching node list
  ndn::Name watcherPrefix(kuaPrefix);
  watcherPrefix.append("sync").append("health");
  kua::NodeWatcher nodeWatcher(watcherPrefix, nodePrefix, face, keyChain);

  // Infinite loop
  face.processEvents();
}