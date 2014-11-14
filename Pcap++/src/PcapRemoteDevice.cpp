#ifdef WIN32

#define LOG_MODULE PcapLogModuleRemoteDevice

#include <PcapRemoteDevice.h>
#include <Logger.h>
#include <pcap.h>

 PcapRemoteDevice::PcapRemoteDevice(pcap_if_t* pInterface, pcap_rmtauth* pRemoteAuthentication) : PcapLiveDevice(pInterface, false)
 {
	 LOG_DEBUG("MTU calculation isn't supported for remote devices. Setting MTU to 1514");
	 m_DeviceMtu = 1514;
	 m_RemoteMachineIpAddress = "";
	 m_RemoteMachinePort = 0;
	 m_pRemoteAuthentication = pRemoteAuthentication;
 }

 PcapRemoteDevice::~PcapRemoteDevice()
 {
	 if (m_pRemoteAuthentication != NULL)
	 {
		 delete [] m_pRemoteAuthentication->username;
		 delete [] m_pRemoteAuthentication->password;
		 delete m_pRemoteAuthentication;
	 }
 }

bool PcapRemoteDevice::open()
{
	char errbuf[PCAP_ERRBUF_SIZE];
	int flags = PCAP_OPENFLAG_PROMISCUOUS | PCAP_OPENFLAG_NOCAPTURE_RPCAP; //PCAP_OPENFLAG_DATATX_UDP doesn't always work
	LOG_DEBUG("Opening device '%s'", m_pName);
	m_pPcapDescriptor = pcap_open(m_pName, MAX_PACKET_SIZE, flags, 250, m_pRemoteAuthentication, errbuf);
	if (m_pPcapDescriptor == NULL)
	{
		LOG_ERROR("Error opening device. Error was: %s", errbuf);
		m_DeviceOpened = false;
		return false;
	}

	//setFilter requires that m_DeviceOpened == true
	m_DeviceOpened = true;

	//for some reason if a filter is not set than WinPcap throws an exception. So Here is a generic filter that catches all traffic
	if (!setFilter("ether proto (\\ip or \\ip6 or \\arp or \\rarp or \\decnet or \\sca or \\lat or \\mopdl or \\moprc or \\iso or \\stp or \\ipx or \\netbeui or 0x80F3)")) //0x80F3 == AARP
	{
		LOG_ERROR("Error setting the filter. Error was: %s", LoggerPP::getInstance().getErrorString());
		m_DeviceOpened = false;
		return false;
	}

	LOG_DEBUG("Device '%s' opened", m_pName);

	return true;
}

void* PcapRemoteDevice::remoteDeviceCaptureThreadMain(void *ptr)
{
	PcapRemoteDevice* pThis = (PcapRemoteDevice*)ptr;
	if (pThis == NULL)
	{
		LOG_ERROR("Capture thread: Unable to extract PcapLiveDevice instance");
		return 0;
	}

	LOG_DEBUG("Started capture thread for device '%s'", pThis->m_pName);

	pcap_pkthdr* pkthdr;
	const uint8_t* pktData;

	if (pThis->m_CaptureCallbackMode)
	{
		while (!pThis->m_StopThread)
		{
			if (pcap_next_ex(pThis->m_pPcapDescriptor, &pkthdr, &pktData) > 0)
				onPacketArrives((uint8_t*)pThis, pkthdr, pktData);
		}
	}
	else
	{
		while (!pThis->m_StopThread)
		{
			if (pcap_next_ex(pThis->m_pPcapDescriptor, &pkthdr, &pktData) > 0)
				onPacketArrivesNoCallback((uint8_t*)pThis, pkthdr, pktData);
		}
	}
	LOG_DEBUG("Ended capture thread for device '%s'", pThis->m_pName);
	return 0;
}

ThreadStart PcapRemoteDevice::getCaptureThreadStart()
{
	return &remoteDeviceCaptureThreadMain;
}

void PcapRemoteDevice::getStatistics(pcap_stat& stats)
{
	int allocatedMemory;
	pcap_stat* tempStats = pcap_stats_ex(m_pPcapDescriptor, &allocatedMemory);
	if (allocatedMemory < (int)sizeof(pcap_stat))
	{
		LOG_ERROR("Error getting statistics from live device '%s': WinPcap did not allocate the entire struct", m_pName);
		return;
	}
	stats.ps_recv = tempStats->ps_capt;
	stats.ps_drop = tempStats->ps_drop + tempStats->ps_netdrop;
	stats.ps_ifdrop = tempStats->ps_ifdrop;
}

#endif // WIN32