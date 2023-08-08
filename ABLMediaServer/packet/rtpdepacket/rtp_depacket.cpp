#include "rtp_depacket.h"
#include "session_manager.h"


int32_t rtp_depacket_start(rtp_depacket_callback cb, void* userdata, uint32_t* h)
{
	if (!cb || !h)
	{
		return e_rtpdepkt_err_invalidparam;
	}

	*h = 0;

	rtp_session_ptr s = gblRtpDepacketMgrGet->malloc(cb, userdata);
	if (!s)
	{
		return e_rtpdepkt_err_mallocsessionfail;
	}

	if (!gblRtpDepacketMgrGet->push(s))
	{
		return e_rtpdepkt_err_managersessionfail;
	}

	*h = s->get_id();

	return e_rtpdepkt_err_noerror;
}

int32_t rtp_depacket_stop(uint32_t h)
{
	if (!gblRtpDepacketMgrGet->pop(h))
	{
		return e_rtpdepkt_err_invalidsessionhandle;
	}

	return e_rtpdepkt_err_noerror;
}

int32_t rtp_depacket_input(uint32_t h, uint8_t* data, uint32_t datasize)
{
	if (!data || (0 == datasize))
	{
		return e_rtpdepkt_err_invalidparam;
	}

	rtp_session_ptr s = gblRtpDepacketMgrGet->get(h);
	if (!s)
	{
		return e_rtpdepkt_err_notfindsession;
	}

	
	return s->handle(data, datasize);
}

int32_t rtp_depacket_setpayload(uint32_t h, uint8_t payload, uint32_t streamtype)
{
	rtp_session_ptr s = gblRtpDepacketMgrGet->get(h);
	if (!s)
	{
		return e_rtpdepkt_err_notfindsession;
	}

	s->set_payload(payload, streamtype);

	return e_rtpdepkt_err_noerror;
}

int32_t rtp_depacket_setmediaoption(uint32_t h, int8_t* opt, int8_t* parm)
{
	if (!opt || (0 == strcmp("", reinterpret_cast<char*>(opt))) || 
		!parm || (0 == strcmp("", reinterpret_cast<char*>(parm))))
	{
		return e_rtpdepkt_err_invalidparam;
	}

	rtp_session_ptr s = gblRtpDepacketMgrGet->get(h);
	if (!s)
	{
		return e_rtpdepkt_err_notfindsession;
	}

	s->set_mediaoption(reinterpret_cast<char*>(opt), reinterpret_cast<char*>(parm));

	return e_rtpdepkt_err_noerror;
}