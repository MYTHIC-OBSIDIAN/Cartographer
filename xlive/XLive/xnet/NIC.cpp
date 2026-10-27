#include "stdafx.h"
#include "NIC.h"

// ### FIXME
IP_ADAPTER_ADDRESSES* CNic::GetNetworkAdapterWithGateway()
{
	return NULL;

/*
#define MAX_ITERATIONS 3

	DWORD dwRetVal = 0;
	// Set the flags to pass to GetAdaptersAddresses.
	ULONG flags = 0;
	ULONG family = AF_INET;
	PIP_ADAPTER_ADDRESSES pAddresses = NULL;

	// get the function to tell us how much memory we need
	ULONG outBufLen = sizeof(IP_ADAPTER_ADDRESSES);
	ULONG Iterations = 0;

	do {
		pAddresses = (IP_ADAPTER_ADDRESSES*)HeapAlloc(GetProcessHeap(), 0, (outBufLen));
		if (pAddresses == NULL) {
			LOG_TRACE_NETWORK("{} memory allocation failed for IP_ADAPTER_ADDRESSES struct.", __FUNCTION__);
			break;
		}

		dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

		if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
			HeapFree(GetProcessHeap(), 0, pAddresses);
			pAddresses = NULL;
		}
		else {
			break;
		}

		Iterations++;
		// 3 attempts max.
	} while ((dwRetVal == ERROR_BUFFER_OVERFLOW) && (Iterations < MAX_ITERATIONS));

	if (dwRetVal == NO_ERROR) {
		for (auto adapters = pAddresses; adapters != nullptr; adapters = adapters->Next) {

			// check if this adapter is ready to send packets
			if (adapters->OperStatus == IfOperStatusUp) {
				for (auto address = adapters->FirstUnicastAddress; address != nullptr; address = address->Next)
				{
					if (address->Address.lpSockaddr->sa_family == AF_INET)
					{
						SOCKADDR_IN* ipv4 = reinterpret_cast<SOCKADDR_IN*>(address->Address.lpSockaddr);

						LOG_TRACE_NETWORK("{} - found local ip address: {}", __FUNCTION__, inet_ntoa(ipv4->sin_addr));
					}
				}
			}
		}
	}
	

	if (pAddresses) {
		HeapFree(GetProcessHeap(), 0, pAddresses);
	}

#undef MAX_ITERATIONS
*/
}

IN_ADDR CNic::GetBestInterfaceRouteAddressFromIp(const char* ipToTest)
{
	DWORD dwIfIndex;
	IN_ADDR address;
	address.S_un.S_addr = 0;

	if (GetBestInterface(inet_addr(ipToTest), &dwIfIndex) == NO_ERROR)
	{
		MIB_IPADDRTABLE* ipAddrTable = (MIB_IPADDRTABLE *)HeapAlloc(GetProcessHeap(), 0, sizeof(MIB_IPADDRTABLE));

		DWORD size = sizeof(MIB_IPADDRTABLE);
		if (GetIpAddrTable(ipAddrTable, &size, 0) == ERROR_INSUFFICIENT_BUFFER)
		{
			HeapFree(GetProcessHeap(), 0, ipAddrTable);
			ipAddrTable = (MIB_IPADDRTABLE *)HeapAlloc(GetProcessHeap(), 0, size);
		}

		if (ipAddrTable == NULL
			|| GetIpAddrTable(ipAddrTable, &size, 0) != NO_ERROR)
		{
			HeapFree(GetProcessHeap(), 0, ipAddrTable);
			return address;
		}

		for (DWORD i = 0; i < ipAddrTable->dwNumEntries; i++)
		{
			if (dwIfIndex == ipAddrTable->table[i].dwIndex)
			{
				address.S_un.S_addr = (ULONG)ipAddrTable->table[i].dwAddr;
				LOG_TRACE_NETWORK("{} - found interface with IPv4 address: {} to route from: {} (0 = INADDR_ANY)", __FUNCTION__, inet_ntoa(address), ipToTest);

				// address found
				break;
			}
		}

		HeapFree(GetProcessHeap(), 0, ipAddrTable);
		return address;
	}

	return address;
}

/* LAN edition: broadcast on all network adapters */

#ifndef MIB_IPADDR_DISCONNECTED
#define MIB_IPADDR_DISCONNECTED 0x0008
#endif
#ifndef MIB_IPADDR_DELETED
#define MIB_IPADDR_DELETED 0x0040
#endif

enum
{
	k_nic_max_broadcast_addresses = 32,
	k_nic_broadcast_cache_ms = 5000,
};

static std::mutex g_nic_broadcast_mutex;
static IN_ADDR g_nic_broadcast_addresses[k_nic_max_broadcast_addresses];
static int g_nic_broadcast_address_count = 0;
static ULONGLONG g_nic_broadcast_last_update = 0;
static bool g_nic_broadcast_cache_valid = false;

static void nic_refresh_broadcast_addresses(void)
{
	g_nic_broadcast_address_count = 0;

	ULONG size = 0;
	if (GetIpAddrTable(NULL, &size, FALSE) != ERROR_INSUFFICIENT_BUFFER || size == 0)
	{
		return;
	}

	MIB_IPADDRTABLE* table = (MIB_IPADDRTABLE*)HeapAlloc(GetProcessHeap(), 0, size);
	if (table == NULL)
	{
		return;
	}

	if (GetIpAddrTable(table, &size, FALSE) == NO_ERROR)
	{
		for (DWORD i = 0; i < table->dwNumEntries && g_nic_broadcast_address_count < k_nic_max_broadcast_addresses; i++)
		{
			const MIB_IPADDRROW* row = &table->table[i];
			const DWORD address = row->dwAddr;	// network byte order
			const DWORD mask = row->dwMask;		// network byte order

			if ((row->wType & (MIB_IPADDR_DISCONNECTED | MIB_IPADDR_DELETED)) != 0
				|| address == 0
				|| (ntohl(address) >> 24) == 127	// loopback
				|| mask == 0
				|| mask == 0xFFFFFFFF)
			{
				continue;
			}

			IN_ADDR broadcast;
			broadcast.s_addr = address | ~mask;

			bool duplicate = false;
			for (int j = 0; j < g_nic_broadcast_address_count; j++)
			{
				if (g_nic_broadcast_addresses[j].s_addr == broadcast.s_addr)
				{
					duplicate = true;
					break;
				}
			}

			if (!duplicate)
			{
				g_nic_broadcast_addresses[g_nic_broadcast_address_count++] = broadcast;
				LOG_TRACE_NETWORK("{} - adapter broadcast address: {}", __FUNCTION__, inet_ntoa(broadcast));
			}
		}
	}

	HeapFree(GetProcessHeap(), 0, table);
	return;
}

int CNic::GetAllInterfaceBroadcastAddresses(IN_ADDR* out_addresses, int max_count)
{
	std::lock_guard<std::mutex> lock(g_nic_broadcast_mutex);

	const ULONGLONG now = GetTickCount64();
	if (!g_nic_broadcast_cache_valid || now - g_nic_broadcast_last_update >= k_nic_broadcast_cache_ms)
	{
		nic_refresh_broadcast_addresses();
		g_nic_broadcast_last_update = now;
		g_nic_broadcast_cache_valid = true;
	}

	int count = 0;
	for (; count < g_nic_broadcast_address_count && count < max_count; count++)
	{
		out_addresses[count] = g_nic_broadcast_addresses[count];
	}
	return count;
}
