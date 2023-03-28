#pragma once


#include <stdint.h>
#include <algorithm> // swap
#include "dpl_Swap.h"
#include "dpl_GeneralException.h"


namespace dpl
{
	/*
		If objects of class T are associated with objects of type U,
		then class T should be derived from Association<T, U> 
		and class U should be derived from Association<U, T>.
	*/
	template<typename ThisT, typename OtherT, uint32_t ID = 0>
	class Association
	{
	protected: // subtypes
		using MyTarget = Association<OtherT, ThisT, ID>;

	public: // friends
		friend MyTarget;

	private: // data
		MyTarget* m_target;

	protected: // lifecycle
		CLASS_CTOR				Association()
			: m_target(nullptr)
		{

		}

		CLASS_CTOR				Association(	const Association&	OTHER) = delete;

		CLASS_CTOR				Association(	Association&&		other) noexcept
			: m_target(other.m_target)
		{
			other.m_target = nullptr;
			notify_moved();
		}

		CLASS_DTOR				~Association()
		{
			dpl::no_except([&](){	unlink();	});
		}

		Association&			operator=(		const Association&	OTHER) = delete;

		Association&			operator=(		Association&&		other) noexcept
		{
			if(this != &other)
			{
				unlink();
				m_target		= other.m_target;
				other.m_target	= nullptr;
				notify_moved();
			}

			return *this;
		}

		Association&			operator=(		Swap<Association>	other)
		{
			std::swap(m_target, other->m_target);
			this->notify_moved();
			other->notify_moved();
			return *this;
		}

		inline Association&		operator=(		Swap<ThisT>			other)
		{
			return operator=(Swap(static_cast<Association&>(*other.get())));
		}

	protected: // functions
		bool					link(			MyTarget&			target)
		{
			if(m_target != &target)
			{
				unlink();
				target.unlink();

				m_target		= &target;
				target.m_target	= this;
				return true;
			}

			return false;
		}

		bool					unlink()
		{
			if(m_target)
			{
				m_target->m_target = nullptr;
				m_target = nullptr;
				return true;
			}

			return false;
		}

		inline bool				relink(			MyTarget&			target)
		{
			unlink();
			return link(target);
		}

		inline OtherT*			other()
		{
			return static_cast<OtherT*>(m_target);
		}

	public: // functions
		inline bool				is_linked() const
		{
			return m_target != nullptr;
		}

		inline const OtherT*	other() const
		{
			return static_cast<const OtherT*>(m_target);
		}

	private: // functions
		inline void				notify_moved()
		{
			if(m_target) m_target->m_target = this;
		}
	};
}