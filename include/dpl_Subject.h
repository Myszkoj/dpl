#pragma once


#include "dpl_Unique.h"
#include "dpl_Membership.h"
#include "dpl_GeneralException.h"


// declarations
namespace dpl
{
	template<typename SubjectT>
	class Subject;

	template<typename SubjectT>
	class Observer;

	constexpr uint32_t SUBJECT_CHAIN_ID = 1000000;
}

// implementations
namespace dpl
{
	/*
		Observable object with build-in unique identifier.
	*/
	template<typename SubjectT>
	class Subject	: public Unique<Subject<SubjectT>> 
					, private Group<Subject<SubjectT>, Observer<SubjectT>, SUBJECT_CHAIN_ID>
	{
	private: // subtypes
		using MyObserver	= Observer<SubjectT>;
		using MyUnique		= Unique<Subject<SubjectT>>;
		using MyChain		= Group<Subject<SubjectT>, MyObserver, SUBJECT_CHAIN_ID>;

	public: // relations
		friend MyObserver;
		friend MyChain;

	protected: // lifecycle
		CLASS_CTOR				Subject() = default;

		CLASS_CTOR				Subject(		const Subject&		OTHER) = delete;

		CLASS_CTOR				Subject(		Subject&&			other) noexcept
			: MyUnique(std::move(other))
			, MyChain(std::move(other))
		{

		}

		CLASS_DTOR				~Subject()
		{
			remove_all_observers();
		}

		inline Subject&			operator=(		const Subject&		OTHER) = delete;

		Subject&				operator=(		Subject&&			other) noexcept
		{
			remove_all_observers();
			MyUnique::operator=(std::move(other));
			MyChain::operator=(std::move(other));
			return *this;
		}

		inline Subject&			operator=(		Swap<Subject>		other)
		{
			MyUnique::operator=(std::move(*other));
			MyChain::operator=(Swap(static_cast<MyChain&>(*other.get())));
			return *this;
		}

		inline Subject&			operator=(		Swap<SubjectT>		other)
		{
			return operator=(Swap<Subject>(*other));
		}

	protected: // functions
		inline bool				is_observed() const
		{
			return MyChain::size() > 0;
		}

		inline void				notify_observers()
		{
			SubjectT& subject = *cast();
			MyChain::for_each_link([&](MyObserver& observer)
			{
				observer.on_update(subject);
			});
		}

		inline void				remove_all_observers()
		{
			while(MyObserver* observer = MyChain::first())
			{
				observer->stop_observation();
			}
		}

	private: // functions
		inline SubjectT*		cast()
		{
			return static_cast<SubjectT*>(this);
		}

		inline const SubjectT*	cast() const
		{
			return static_cast<const SubjectT*>(this);
		}

	private: // interface
		virtual void			on_observers_changed(){}
	};


	/*
		Person/device that observes the Subject.
	*/
	template<typename SubjectT>
	class Observer : private Member<Subject<SubjectT>, Observer<SubjectT>, SUBJECT_CHAIN_ID>
	{
	private: // subtypes
		using MySubject = Subject<SubjectT>;
		using MyBase	= Member<MySubject, Observer<SubjectT>, SUBJECT_CHAIN_ID>;

	public: // relations
		friend MySubject;
		friend MyBase;
		friend Group<Subject<SubjectT>, Observer<SubjectT>, SUBJECT_CHAIN_ID>;
		friend Sequenceable<Observer<SubjectT>, SUBJECT_CHAIN_ID>;

	protected: // lifecycle
		CLASS_CTOR					Observer() = default;

		CLASS_CTOR					Observer(			const Observer&		OTHER) = delete;

		CLASS_CTOR					Observer(			Observer&&			other) noexcept
			: MyBase(std::move(other))
		{

		}

		CLASS_DTOR					~Observer()
		{
			dpl::no_except([&]()
			{
				if(MySubject* subject = MyBase::get_chain())
				{
					MyBase::detach();
					subject->on_observers_changed();
				}
			});
		}

		inline Observer&			operator=(			const Observer&		OTHER) = delete;

		Observer&					operator=(			Observer&&			other) noexcept
		{
			stop_observation();
			MyBase::operator=(std::move(other));
			return *this;
		}

		inline Observer&			operator=(			Swap<Observer>		other)
		{
			MyBase::operator=(Swap(static_cast<MyBase&>(*other.get())));
			return *this;
		}

	protected: // functions
		inline void					observe(			MySubject&			subject)
		{
			if(!MyBase::is_linked(subject))
			{
				subject.attach_back(*this);
				subject.on_observers_changed();
				this->on_observe(*subject.cast());
			}
		}

		inline void					stop_observation()
		{
			if(MySubject* subject = MyBase::get_chain())
			{
				const uint32_t SUBJECT_ID = subject->ID();
				MyBase::detach();
				subject->on_observers_changed();
				on_subject_lost(nullptr, SUBJECT_ID);
			}
		}

		inline bool					has_subject() const
		{
			return MyBase::is_linked();
		}

		inline SubjectT*			get_subject()
		{
			Subject<SubjectT>* subject = MyBase::get_chain();
			return subject ? subject->cast() : nullptr;
		}

		inline const SubjectT*		get_subject() const
		{
			const Subject<SubjectT>* SUBJECT = MyBase::get_chain();
			return SUBJECT ? SUBJECT->cast() : nullptr;
		}

	private: // interface
		virtual void				on_observe(			SubjectT&			subject){}

		virtual void				on_update(			SubjectT&			subject){}

		virtual void				on_subject_lost(	const SubjectT*		ALWAYS_NULL,
														const uint32_t		UNIQUE_ID){}
	};
}