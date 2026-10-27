#pragma once

class CNic
{
public:
	static IP_ADAPTER_ADDRESSES* GetNetworkAdapterWithGateway();
	static IN_ADDR GetBestInterfaceRouteAddressFromIp(const char* ipToTest);

	// LAN edition: directed (subnet) broadcast address of every connected IPv4 interface,
	// so System Link broadcasts go out of all network adapters like XLiveLessNess.
	// Cached for a few seconds. Returns the number of addresses written.
	static int GetAllInterfaceBroadcastAddresses(IN_ADDR* out_addresses, int max_count);
};