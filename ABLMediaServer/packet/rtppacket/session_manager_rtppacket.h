#pragma once
#include <unordered_map>
#include <mutex>
#include "../../HSingleton.h"


#include "session.h"


class rtp_session_manager_rtppacket
{
public:
	rtp_session_ptr malloc(rtp_packet_callback cb, void* userdata);
	void free(rtp_session_ptr p);

	bool push(rtp_session_ptr s);
	bool pop(uint32_t h);
	rtp_session_ptr get(uint32_t h);

private:
	std::unordered_map<uint32_t, rtp_session_ptr> m_sessionmap;


	std::mutex m_sesmapmtx;


};

#define gblRtppacketMgrGet HSingletonTemplatePtr<rtp_session_manager_rtppacket>::Instance()
