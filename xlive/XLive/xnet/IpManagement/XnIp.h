#pragma once 

#include "../xnet.h"
#include "../Sockets/XSocket.h"

#include "../net_utils.h"


#define XNIP_MAX_PCK_STR_HDR_LEN 32
#define XNIP_MAX_NET_STATS_SAMPLES 30

#define XNIP_QOS_PORT_OFFSET 10

enum 
{
	EXNIP_PACKET_SIGNATURE_XNET_REQUEST		= 'XNeT',
	EXNIP_PACKET_SIGNATURE_XNET_BROADCAST	= 'BrOd',
};

#define XNIP_REQUEST_HEADER_STR		"XNetReqPack"
#define XNIP_BROADCAST_HEADER_STR	"XNetBrOadPack"

#define XnIp_ConnectionIndexMask 0xFF000000

#define XnIp_ConnectionTimeOut (15 * 1000) // msec

#define XNIP_FLAG(_bit) (1<<(_bit))
#define XNIP_SET_BIT(_flags, _bit, _value) ((_value) ? ((_flags) |= XNIP_FLAG((_bit))) : ((_flags) &= ~(XNIP_FLAG(_bit))))
#define XNIP_TEST_BIT(_flags, _bit) (((_flags) & XNIP_FLAG((_bit))) != 0)

TEST_N_DEF(XL0);

enum eXnip_ConnectRequestType : int
{
	EXNIP_CONNECTION_REQUEST_INVALID = -1,

	EXNIP_CONNECTION_PING,
	EXNIP_CONNECTION_PONG,
	EXNIP_CONNECTION_PORT_MAPPING_UPDATE,
	EXNIP_CONNECTION_FINISH_ESTABLISH_SECURE_CHANNEL,
	EXNIP_CONNECTION_ACKNOWLEDGE_CONNECTED_SECURE_CHANNEL,
	EXNIP_CONNECTION_CLOSE_SECURE
};

enum eXnIp_ConnectionRequestBitFlags
{
	XnIp_HasPortMappingsUpdated = 0,
};

#pragma region NAT handling

// TODO: currently we use multiple system sockets
// to send data to the corresponding virtual socket
// when in practice we could just use one
struct PortMapping
{
	enum class PortMapState : int
	{
		XNIP_NET_ADDRESS_MAPPINGS_UNAVAILABLE,
		XNIP_NET_ADDRESS_MAP_AVAILABLE,
	};

	PortMapState state;
	sockaddr_in address;
	WORD virtualPort;
};

#pragma endregion

struct XNetPacketHeader
{
	DWORD signature;
	char signatureString[XNIP_MAX_PCK_STR_HDR_LEN];
};

struct XBroadcastPacket
{
	XBroadcastPacket()
	{
		pckHeader.signature = EXNIP_PACKET_SIGNATURE_XNET_BROADCAST;
		strncpy_s(pckHeader.signatureString, XNIP_BROADCAST_HEADER_STR, XNIP_MAX_PCK_STR_HDR_LEN);
		ZeroMemory(&data, sizeof(data));
		data.titleId = (DWORD)-1;
		data.name.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	};

	XNetPacketHeader pckHeader;
	struct
	{
		DWORD titleId;
		sockaddr_in name;
	} data;
};

// LAN edition: like XLiveLessNess, learn the address each System Link peer's
// broadcasts actually arrive from, and use it instead of the single LAN address
// the peer advertises (which may be on a different network adapter).
// The sender's abEnet is carried in the unused sin_zero bytes of XBroadcastPacket::data.name.
void xnip_system_link_tag_broadcast(XBroadcastPacket* packet);
void xnip_system_link_learn_peer_address(const XBroadcastPacket* packet, IN_ADDR from_address);
bool xnip_system_link_get_peer_address(const BYTE* abEnet, IN_ADDR* out_address);

struct XNetRequestPacket
{
	XNetRequestPacket()
	{
		pckHeader.signature = EXNIP_PACKET_SIGNATURE_XNET_REQUEST;
		memset(pckHeader.signatureString, 0, sizeof(pckHeader.signatureString));
		strncpy_s(pckHeader.signatureString, XNIP_REQUEST_HEADER_STR, XNIP_MAX_PCK_STR_HDR_LEN);
		ZeroMemory(&data, sizeof(data));
	}

	XNetPacketHeader pckHeader;
	struct
	{
		XNADDR xnaddr;
		XNKID xnkid;
		BYTE nonceKey[8];
		eXnip_ConnectRequestType reqType;
		union
		{
			struct // EXNIP_CONNECTION_PORT_MAPPING_UPDATE EXNIP_CONNECTION_FINISH_ESTABLISH_SECURE_CHANNEL
			{
				DWORD flags;
				bool connectionInitiator;
				WORD senderVirtualPort;
			};
		};
	} data;
};

struct XnKeyPair
{
	bool m_valid;
	XNKID m_xnkid;
	XNKEY m_xnkey;
};

struct XnIpPckTransportStats
{
	bool initialized;

	unsigned int pckSent;
	unsigned int pckRecvd;
	unsigned int pckBytesSent;
	unsigned int pckBytesRecvd;

	unsigned int pckSentPerSec[XNIP_MAX_NET_STATS_SAMPLES];
	unsigned int pckBytesSentPerSec[XNIP_MAX_NET_STATS_SAMPLES];

	unsigned int pckRecvdPerSec[XNIP_MAX_NET_STATS_SAMPLES];
	unsigned int pckBytesRecvdPerSec[XNIP_MAX_NET_STATS_SAMPLES];

	int			 pckCurrentSendPerSecIdx;
	int			 pckCurrentRecvdPerSecIdx;

	ULONGLONG	 lastPacketReceivedTime;

	void PckDataSampleUpdate()
	{
		if (!initialized)
		{
			initialized = true;
			pckSent = 0;
			pckRecvd = 0;
			pckBytesSent = 0;
			pckBytesRecvd = 0;

			pckSentPerSecIdx = 0;
			pckRecvdPerSecIdx = 0;
			pckCurrentSendPerSecIdx = -1;
			pckCurrentRecvdPerSecIdx = -1;

			memset(pckSentPerSec, 0, sizeof(pckSentPerSec));
			memset(pckRecvdPerSec, 0, sizeof(pckRecvdPerSec));

			lastTimeUpdate = timeGetTime();
		}
		else
		{
			const ULONGLONG sample_end_time = 1ull * 1000ull;

			if (timeGetTime() - lastTimeUpdate >= sample_end_time)
			{
				pckSentPerSecIdx = (pckSentPerSecIdx + 1) % XNIP_MAX_NET_STATS_SAMPLES;
				pckRecvdPerSecIdx = (pckRecvdPerSecIdx + 1) % XNIP_MAX_NET_STATS_SAMPLES;

				pckSentPerSec[pckSentPerSecIdx] = 0;
				pckBytesSentPerSec[pckSentPerSecIdx] = 0;

				pckRecvdPerSec[pckRecvdPerSecIdx] = 0;
				pckBytesRecvdPerSec[pckRecvdPerSecIdx] = 0;

				pckCurrentSendPerSecIdx = (pckCurrentSendPerSecIdx + 1) % XNIP_MAX_NET_STATS_SAMPLES;
				pckCurrentRecvdPerSecIdx = (pckCurrentRecvdPerSecIdx + 1) % XNIP_MAX_NET_STATS_SAMPLES;

				lastTimeUpdate = timeGetTime();
			}
		}
	}

	void PckSendStatsUpdate(unsigned int _pckXmit, unsigned int _pckXmitBytes)
	{
		PckDataSampleUpdate();

		pckSent += _pckXmit;
		pckBytesSent += _pckXmitBytes;

		pckSentPerSec[pckSentPerSecIdx] += _pckXmit;
		pckBytesSentPerSec[pckSentPerSecIdx] += _pckXmitBytes;
	}

	void PckRecvdStatsUpdate(unsigned int _pckRecvd, unsigned int _pckRecvdBytes)
	{
		PckDataSampleUpdate();

		pckRecvd += _pckRecvd;
		pckBytesRecvd += _pckRecvdBytes;

		pckRecvdPerSec[pckRecvdPerSecIdx] += _pckRecvd;
		pckBytesRecvdPerSec[pckRecvdPerSecIdx] += _pckRecvdBytes;

		lastPacketReceivedTime = timeGetTime();
	}

private:
	ULONGLONG lastTimeUpdate;
	int pckSentPerSecIdx;
	int pckRecvdPerSecIdx;
};

struct XnIp
{
	IN_ADDR m_connectionId;
	XNADDR m_xnaddr;
	// key we connected with
	XnKeyPair* m_keyPair;

	TEST_N_DEF(XL4);

	bool m_valid;
	int m_connectStatus;
	int m_connectionPacketsSentCount;
	
	ULONGLONG m_lastConnectionInteractionTime;

	BYTE m_nonce[8];
	BYTE m_endpointNonce[8];
	bool m_endpointNonceValid;

	bool m_requestContext;

	NetLinkedList m_netAddrMappings;

	// describes if this connection was created
	// in the event of a received packet
	// if true, the endpoint initiated the connection
	// if false, local machine attempted to connect
	bool m_connectionInitiator;

	enum eXnIp_Flags
	{
		XnIp_ConnectDeclareConnectedRequestSent,
		XnIp_ReconnectionAttempt,
	};
	int m_flags;

public:
	XnIpPckTransportStats m_pckStats;

	void PckStatsReset()
	{
		m_pckStats.initialized = false;
	}

	bool PckGetStats(const XnIpPckTransportStats** outPckStats) const
	{
		bool result = false;
		if (m_pckStats.initialized)
		{
			*outPckStats = &m_pckStats;
			result = true;
		}

		return result;
	}

	IN_ADDR GetOnlineIpAddr() const
	{
#ifdef XL5
		TEST_N_DEF(XL5);
#else
		return m_xnaddr.inaOnline;
#endif
	}

	IN_ADDR GetLanIpAddr() const
	{
		return m_xnaddr.ina;
	}

	bool IsValid(IN_ADDR identifier) const
	{
		bool valid = m_valid
			&& identifier.s_addr == GetConnectionId().s_addr;

		if (!valid)
		{
			LOG_CRITICAL_NETWORK("{} - m_valid: {} or {:X} != {:X}", __FUNCTION__, m_valid, identifier.s_addr, GetConnectionId().s_addr);
			return false;
		}

		return valid;
	}

	void UpdateInteractionTimeHappened()
	{
		m_lastConnectionInteractionTime = timeGetTime();
	}

	IN_ADDR GetConnectionId() const
	{
		return m_connectionId;
	}

	bool InitiatedConnectRequest() const 
	{
		return m_connectionInitiator;
	}

	int GetConnectStatus() const
	{
		return m_connectStatus;
	}

	void SetConnectStatus(int connectStatus)
	{
		m_connectStatus = connectStatus;
	}

	bool ConnectStatusIdle() const
	{
		return GetConnectStatus() == XNET_CONNECT_STATUS_IDLE;
	}

	bool ConnectStatusPending() const
	{
		return GetConnectStatus() == XNET_CONNECT_STATUS_PENDING;
	}

	bool ConnectStatusConnected() const 
	{
		return GetConnectStatus() == XNET_CONNECT_STATUS_CONNECTED;
	}

	bool ConnectStatusLost() const
	{
		return GetConnectStatus() == XNET_CONNECT_STATUS_LOST;
	}

	bool ConnectionTimedOut() const 
	{
		return timeGetTime() - m_lastConnectionInteractionTime >= XnIp_ConnectionTimeOut;
	}

	static int GetConnectionIndex(IN_ADDR connectionId);

	void SavePortMapping(XVirtualSocket* xsocket, WORD virtualPort, const sockaddr_in* addr) const;
	void HandleConnectionPacket(XVirtualSocket* xsocket, const XNetRequestPacket* reqPacket, const sockaddr_in* recvAddr, LPDWORD lpBytesRecvdCount);
	void HandleDisconnectPacket(XVirtualSocket* xsocket, const XNetRequestPacket* disconnectReqPck, const sockaddr_in* recvAddr) const; // TODO:
	void UpdateNonceKeyFromPacket(const XNetRequestPacket* reqPacket);

	/* sends a request over the socket to the other socket end, with the same identifier */
	void SendXNetRequest(XVirtualSocket* xsocket, eXnip_ConnectRequestType reqType);

	/* sends a request to all open sockets */
	void SendXNetRequestAllSockets(eXnip_ConnectRequestType reqType);

	void InsertPortMapping(PortMapping* mapping)
	{
		m_netAddrMappings.insert((void*)mapping, sizeof(*mapping));
	}

	const sockaddr_in* GetPortMapping(WORD virtualPort) const
	{
		NetElement* elem = m_netAddrMappings.first();
		while (elem)
		{
			PortMapping* mapping = (PortMapping*)elem->data;
			if (mapping->virtualPort == virtualPort)
			{
				return &mapping->address;
			}

			elem = elem->next;
		}

		return NULL;
	}

	void UpdatePortMapping(WORD virtualPort, const sockaddr_in* addr) const
	{
		NetElement* elem = m_netAddrMappings.first();
		while (elem)
		{
			PortMapping* mapping = (PortMapping*)elem->data;
			if (mapping->virtualPort == virtualPort)
			{
				mapping->state = PortMapping::PortMapState::XNIP_NET_ADDRESS_MAP_AVAILABLE;
				memcpy(&mapping->address, addr, sizeof(mapping->address));
				break;
			}

			elem = elem->next;
		}
	}

	bool PortMappingAvailable(WORD virtualPort) const
	{
		NetElement* elem = m_netAddrMappings.first();

		bool result = false;
		while (elem)
		{
			PortMapping* mapping = (PortMapping*)elem->data;
			if (mapping->virtualPort == virtualPort
				&& mapping->state == PortMapping::PortMapState::XNIP_NET_ADDRESS_MAP_AVAILABLE)
			{
				return true;
			}

			elem = elem->next;
		}

		return result;
	}

	bool PortMappingsAvailable() const
	{
		NetElement* elem = m_netAddrMappings.first();

		bool result = true;
		if (!elem)
		{
			return false;
		}

		while (elem)
		{
			PortMapping* mapping = (PortMapping*)elem->data;
			if (mapping->state != PortMapping::PortMapState::XNIP_NET_ADDRESS_MAP_AVAILABLE)
			{
				return false;
			}

			elem = elem->next;
		}

		return result;
	}

	void DiscardPortMappings()
	{
		m_netAddrMappings.dispose();
	}
};

class XnIpManager
{
public:

	XnIpManager()
	{
		memset(&m_startupParams, 0, sizeof(m_startupParams));
	}

	// TODO maybe terminate all connections
	~XnIpManager() = default;

	// disable copy/move
	XnIpManager(const XnIpManager& other) = delete;
	XnIpManager(XnIpManager&& other) = delete;

	void Initialize(const XNetStartupParams* netStartupParams);
	void Dispose();

	// Connection data getters 
	XnIp* GetConnection(const IN_ADDR ina) const;
	int GetEstablishedConnectionIdentifierByRecvAddr(XVirtualSocket* xsocket, const sockaddr_in* addr, IN_ADDR* outConnectionIdentifier) const;

	// Miscellaneous
	void ClearLostConnections();

	// local network address
	static XnIp* GetLocalUserXn();
	static u_short GetQoSPort();

	static void UnregisterLocalConnectionInfo();
	static void SetupLocalConnectionInfo(unsigned long xnaddr, unsigned long lanaddr, unsigned short baseport, const BYTE* abEnet, const BYTE* abOnline);

	// Performance counters
	void UpdatePacketReceivedCounters(IN_ADDR ipIdentifier, unsigned int bytesRecvdCount) const;

	// Packet handlers
	int HandleRecvdPacket(XVirtualSocket* xsocket, sockaddr_in* lpFrom, WSABUF* lpBuffers, DWORD dwBufferCount, LPDWORD bytesRecvdCount);
	void HandleXNetRequestPacket(XVirtualSocket* xsocket, const XNetRequestPacket* reqPaket, const sockaddr_in* recvAddr, LPDWORD lpBytesRecvdCount);
	void HandleDisconnectPacket(XVirtualSocket* xsocket, const XNetRequestPacket* disconnectReqPck, const sockaddr_in* recvAddr) const;

	// XnIp handling function
	XnIp* XnIpLookup(const XNADDR* pxna, const XNKID* xnkid) const;
	int CreateOrGetXnIpIdentifierFromPacket(const XNADDR* pxna, const XNKID* xnkid, const XNetRequestPacket* reqPacket, IN_ADDR* outIpIdentifier);
	int RegisterNewXnIp(const XNADDR* pxna, const XNKID* pxnkid, IN_ADDR* outIpIdentifier);
	void UnregisterXnIpIdentifier(const IN_ADDR ina) const;

	// Key functions
	int RegisterKey(XNKID*, XNKEY*);
	void UnregisterKey(const XNKID* xnkid);
	XnKeyPair* KeyPairLookup(const XNKID* xnkid) const;
	
	// Logging 
#ifdef TERMINAL_ENABLED
	void LogConnectionsToConsole(int(__cdecl* outputCb)(int, const char*, ...)) const;
#endif
	void LogConnectionsErrorDetails(const sockaddr_in* address, int errorCode, const XNKID* receivedKey) const;

	// XNet startup parameters
	int GetMaxXnConnections()				const { return m_startupParams.cfgSecRegMax; }
	int GetReqQoSBufferSize()				const { return m_startupParams.cfgQosDataLimitDiv4 * 4; }
	int GetMaxXnKeyPairs()					const { return m_startupParams.cfgKeyRegMax; }
	int GetMinSockRecvBufferSizeInBytes()	const { return m_startupParams.cfgSockDefaultRecvBufsizeInK * SOCK_K_UNIT; }
	int GetMinSockSendBufferSizeInBytes()	const { return m_startupParams.cfgSockDefaultSendBufsizeInK * SOCK_K_UNIT; }

	int GetRegisteredKeyCount() const
	{
		int keysCount = 0;
		for (int i = 0; i < GetMaxXnKeyPairs(); i++)
		{
			if (m_XnKeyPairs[i].m_valid)
			{
				keysCount++;
			}
		}

		return keysCount;
	}

	// Data
	XnIp* m_XnIPs = nullptr;
	XnKeyPair* m_XnKeyPairs = nullptr;

private:
	static XnIp m_ipLocal;
	XNetStartupParams m_startupParams;
};

extern XnIpManager gXnIpMgr;

int WINAPI XNetRegisterKey(XNKID *pxnkid, XNKEY *pxnkey);