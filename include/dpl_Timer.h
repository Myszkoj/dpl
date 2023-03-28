#pragma once


#include <iostream>
#include <chrono>
#include <ctime>
#include <string>
#include "dpl_Mask.h"


namespace dpl
{
#pragma pack(push, 4)
	class Timer
	{
	public: // subtypes
		using TimePoint		= std::chrono::time_point<std::chrono::system_clock>;
		using Nanoseconds	= std::chrono::duration<double, std::nano>;
		using Microseconds	= std::chrono::duration<double, std::micro>;
		using Milliseconds	= std::chrono::duration<double, std::milli>;
		using Seconds		= std::chrono::duration<double>;
		using Minutes		= std::chrono::duration<double, std::ratio<60>>;
		using Hours			= std::chrono::duration<double, std::ratio<3600>>;

		template<typename T>
		struct Suffix;

		template<> struct Suffix<Nanoseconds>{	static const char* get(){return "ns";}};
		template<> struct Suffix<Microseconds>{	static const char* get(){return "us";}};
		template<> struct Suffix<Milliseconds>{	static const char* get(){return "ms";}};
		template<> struct Suffix<Seconds>{		static const char* get(){return "s";}};
		template<> struct Suffix<Minutes>{		static const char* get(){return "min";}};
		template<> struct Suffix<Hours>{		static const char* get(){return "h";}};

		enum Flags
		{
			STARTED,
			PAUSED
		};

	public: // lifecycle
		/*CTOR */					Timer()
			: m_clock(0.0)
		{

		}

	public: // functions
		static inline TimePoint		now()
		{
			return std::chrono::system_clock::now();
		}

		static std::string			date()
		{
			std::time_t		tmp = std::chrono::system_clock::to_time_t(now());
			const size_t	MAX_CHARACTERS = 50; 
			char			str[MAX_CHARACTERS];
			ctime_s(str, MAX_CHARACTERS, &tmp);
			return std::string(str);
		}

		inline void					start()
		{
			stop();
			m_state.set_at(STARTED, true);
			m_startPoint = now();
		}

		inline void					stop()
		{
			m_state.set_at(STARTED, false);
			m_state.set_at(PAUSED,	false);
			m_clock = Seconds();
		}

		inline void					pause()
		{
			if (!is_started()) return;
			if (is_paused()) return;
			m_state.set_at(PAUSED, true);
			m_clock	+= Seconds(now() - m_startPoint);
		}

		inline void					unpause()
		{
			if(!is_paused()) return;
			m_state.set_at(PAUSED, false);
			m_startPoint = now();
		}

		inline Seconds				seconds() const
		{
			return (is_started() && !is_paused()) ? m_clock + Seconds(now() - m_startPoint) : m_clock;
		}

		template<typename DurationT>
		inline DurationT			duration() const
		{
			return std::chrono::duration_cast<DurationT>(seconds());
		}

		template<typename DurationT>
		inline std::string			duration_str() const
		{
			return std::to_string(duration<DurationT>().count()) + Suffix<DurationT>::get();
		}

		inline bool					is_started() const
		{
			return m_state.at(STARTED);
		}

		inline bool					is_paused() const
		{
			return m_state.at(PAUSED);
		}

	private:
		Seconds			m_clock;
		TimePoint		m_startPoint;
		Mask32<Flags>	m_state;
	};
#pragma pack(pop)
}