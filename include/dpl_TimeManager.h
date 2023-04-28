#pragma once


#include "dpl_Timer.h"
#include "dpl_ReadOnly.h"


namespace dpl
{
	class TimeManager
	{
	public:		// [DATA]
		dpl::ReadOnly<dpl::Timer,			TimeManager> timer;
		dpl::ReadOnly<dpl::Timer::Seconds,	TimeManager> lastTick;	// Last measured time point
		dpl::ReadOnly<float,				TimeManager> dt;
		dpl::ReadOnly<float,				TimeManager> T;			// Time since last tick
		dpl::ReadOnly<uint64_t,				TimeManager> cycle;
		dpl::ReadOnly<uint32_t,				TimeManager> fpsCounter;// Frames since last tick
		dpl::ReadOnly<uint32_t,				TimeManager> fps;		// Last recorded fps
		dpl::ReadOnly<bool,					TimeManager> bTick;		// true after each second
		
	public:		// [LIFECYCLE]
		CLASS_CTOR		TimeManager()
			: lastTick(0)
			, dt(0.f)
			, T(0.f)
			, cycle(0)
			, fpsCounter(0)
			, fps(0)
			, bTick(true)
		{

		}

	protected:	// [FUNCTIONS]
		void			update()
		{
			if(timer->is_started())
			{
				auto currentTick = timer->seconds();

				dt = static_cast<float>((currentTick - lastTick()).count());
				T = T + dt;
				if(T >= 1.0)
				{			
					fps			= fpsCounter;
					fpsCounter	= 0;
					T			= 0.f;
					bTick		= true;
				}
				else
				{
					bTick		= false;
				}

				lastTick = currentTick;
			}
			else
			{
				timer->start();
				lastTick = 0.0;
				dt	= 0.f;
				T	= 0.f;
			}
		
			++(*cycle);
			++(*fpsCounter);
		}

		void			reset()
		{
			timer->stop();
			lastTick	= 0.0;
			dt			= 0.f;
			T			= 0.f;
			cycle		= 0;
			fpsCounter	= 0;
			fps			= 0;	
			bTick		= false;
		}
	};
}
