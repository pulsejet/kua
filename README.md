# Kua

## Usage

Install ndn-cxx, NFD and ndn-svs, then build as
```
./waf configure
./waf
```

Set Sync prefix to multicast
```
nfdc strategy set /kua/sync/ /localhost/nfd/strategy/multicast
```

Run Kua master
```
nfdc cs erase / && NDN_LOG="kua.*=DEBUG" ./build/bin/kua /kua /master 1
```

Run Kua nodes
```
NDN_LOG="kua.*=DEBUG" ./build/bin/kua /kua /one 0    # on node 1
NDN_LOG="kua.*=DEBUG" ./build/bin/kua /kua /two 0    # on node 2
NDN_LOG="kua.*=DEBUG" ./build/bin/kua /kua /three 0  # on node 3
```
