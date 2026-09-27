#include <NetworkSupport.h>
#include <cassert>
#include <cstdio>

int main()
{
    assert(!NetworkSupport::online());
    assert(NetworkSupport::interfaceIndex() == -1);
    Netif stationHandle, ethernetHandle, pppHandle;
    NetworkInterface station {true, false, &stationHandle, 1, IPAddress(123), "sta"};
    Network.selected = &station;
    assert(!NetworkSupport::online()); // Association before DHCP is not readiness.
    station.address = true;
    assert(NetworkSupport::online());
    assert(NetworkSupport::localIP() == IPAddress(123));
    NetworkInterface ap {true, true, &accessPoint, 2, IPAddress(456), "ap"};
    Network.selected = &ap;
    assert(!NetworkSupport::online()); // AP-only boot cannot open fieldbus services.
    NetworkInterface ethernet {true, true, &ethernetHandle, 3, IPAddress(123), "eth"};
    Network.selected = &ethernet;
    station.link = false;
    assert(NetworkSupport::online());                          // Loss of unrelated Wi-Fi preserves Ethernet.
    assert(NetworkSupport::interfaceIndex() != station.index); // Same IP, different route.
    ethernet.link = false;
    assert(!NetworkSupport::online()); // Stale IP alone cannot keep a service online.
    NetworkInterface ppp {true, true, &pppHandle, 4, IPAddress(789), "ppp"};
    Network.selected = &ppp;
    assert(NetworkSupport::online());
    assert(NetworkSupport::interfaceName() == "ppp");
    puts("PASS: Network uplink readiness, AP exclusion, disconnect and interface failover");
}
