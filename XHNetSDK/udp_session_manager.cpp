#include "udp_session_manager.h"


udp_session_manager::udp_session_manager()
{
}


udp_session_manager::~udp_session_manager()
{
}

bool udp_session_manager::push_udp_session(udp_session_ptr& s)
{
#ifdef LIBNET_USE_CORE_SYNC_MUTEX
	auto_lock::al_lock<auto_lock::al_mutex> al(m_mutex);
#else
	auto_lock::al_lock<auto_lock::al_spin> al(m_mutex);
#endif

	if (s)
	{
#ifdef USE_BOOST
		std::pair<boost::unordered_map<NETHANDLE, udp_session_ptr>::iterator, bool> ret = m_sessions.insert(std::make_pair(s->get_id(), s));

#else
		auto ret = m_sessions.insert(std::make_pair(s->get_id(), s));
#endif
	return ret.second;
	}

	return false;
}

bool udp_session_manager::pop_udp_session(NETHANDLE id)
{
#ifdef LIBNET_USE_CORE_SYNC_MUTEX
	auto_lock::al_lock<auto_lock::al_mutex> al(m_mutex);
#else
	auto_lock::al_lock<auto_lock::al_spin> al(m_mutex);
#endif

	std::map<NETHANDLE, udp_session_ptr>::iterator iter = m_sessions.find(id);
	if (m_sessions.end() != iter)
	{
		if (iter->second)
		{
			iter->second->close();
		}
		m_sessions.erase(iter);

		return true;
	}

	return false;
}

void udp_session_manager::pop_all_udp_sessions()
{
#ifdef LIBNET_USE_CORE_SYNC_MUTEX
	auto_lock::al_lock<auto_lock::al_mutex> al(m_mutex);
#else
	auto_lock::al_lock<auto_lock::al_spin> al(m_mutex);
#endif

	for (boost::unordered_map<NETHANDLE, udp_session_ptr>::iterator iter = m_sessions.begin(); m_sessions.end() != iter; )
	{
		if (iter->second)
		{
			iter->second->close();
		}

		m_sessions.erase(iter++);
	}
}

udp_session_ptr udp_session_manager::get_udp_session(NETHANDLE id)
{
#ifdef LIBNET_USE_CORE_SYNC_MUTEX
	auto_lock::al_lock<auto_lock::al_mutex> al(m_mutex);
#else
	auto_lock::al_lock<auto_lock::al_spin> al(m_mutex);
#endif

	udp_session_ptr s;
	boost::unordered_map<NETHANDLE, udp_session_ptr>::iterator iter = m_sessions.find(id);
	if (m_sessions.end() != iter)
	{
		s = iter->second;
	}

	return s;
}