#pragma once


#include "dpl_Unique.h"
#include "dpl_Membership.h"
#include "dpl_GeneralException.h"


// declarations
namespace dpl
{
	inline constexpr uint32_t SUBJECT_GROUP_ID = 1000000;


	template<typename SubjectT>
	class Subject;

	template<typename SubjectT>
	class Observer;
}

// implementations
namespace dpl
{
	/*
		Observable object with build-in unique identifier.
	*/
	template<typename SubjectT>
	class Subject	: public Unique<Subject<SubjectT>> 
					, private Group<Subject<SubjectT>, Observer<SubjectT>, SUBJECT_GROUP_ID>
	{
	private:	// [SUBTYPES]
		using	MyObserver	= Observer<SubjectT>;
		using	MyUnique	= Unique<Subject<SubjectT>>;
		using	MyGroup		= Group<Subject<SubjectT>, MyObserver, SUBJECT_GROUP_ID>;

	public:		// [FRIENDS]
		friend	MyObserver;
		friend	MyObserver::MyBase;
		friend	MyGroup;

	protected:	// [LIFECYCLE]
		CLASS_CTOR				Subject() = default;

		CLASS_CTOR				Subject(		const Subject&		OTHER) = delete;

		CLASS_CTOR				Subject(		Subject&&			other) noexcept
			: MyUnique(std::move(other))
			, MyGroup(std::move(other))
		{

		}

		CLASS_DTOR				~Subject()
		{
			dpl::no_except([&](){ remove_all_observers(); });
		}

		Subject&				operator=(		const Subject&		OTHER) = delete;

		Subject&				operator=(		Subject&&			other) noexcept
		{
			remove_all_observers();
			MyUnique::operator=(std::move(other));
			MyGroup::operator=(std::move(other));
			return *this;
		}

		Subject&				operator=(		Swap<Subject>		other)
		{
			MyUnique::operator=(std::move(*other));
			MyGroup::operator=(Swap(static_cast<MyGroup&>(*other.get())));
			return *this;
		}

		Subject&				operator=(		Swap<SubjectT>		other)
		{
			return operator=(Swap<Subject>(*other));
		}

	protected:	// [FUNCTIONS]
		bool					is_observed() const
		{
			return MyGroup::size() > 0;
		}

		void					notify_observers()
		{
			SubjectT& subject = *cast();
			MyGroup::for_each_link([&](MyObserver& observer)
			{
				observer.on_update(subject);
			});
		}

		void					remove_all_observers()
		{
			while(MyObserver* observer = MyGroup::first())
			{
				observer->stop_observation();
			}
		}

	private:	// [FUNCTIONS]
		SubjectT*				cast()
		{
			return static_cast<SubjectT*>(this);
		}

		const SubjectT*			cast() const
		{
			return static_cast<const SubjectT*>(this);
		}

	private:	// [INTERFACE]
		virtual void			on_observers_changed(){}
	};


	/*
		Person/device that observes the Subject.
	*/
	template<typename SubjectT>
	class Observer : private Member<Subject<SubjectT>, Observer<SubjectT>, SUBJECT_GROUP_ID>
	{
	private:	// [SUBTYPES]
		using	MySubject	= Subject<SubjectT>;
		using	MyBase		= Member<MySubject, Observer<SubjectT>, SUBJECT_GROUP_ID>;

	public:		// [SUBTYPES]
		using	MyLink		= Link<Observer<SubjectT>, SUBJECT_GROUP_ID>;

	public:		// [FRIENDS]
		friend	MySubject;
		friend	MyBase;
		friend	MyLink;

		friend	Group<Subject<SubjectT>, Observer<SubjectT>, SUBJECT_GROUP_ID>;
		friend	Sequenceable<Observer<SubjectT>, SUBJECT_GROUP_ID>;

	protected:	// [LIFECYCLE]
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
				if(MySubject* subject = MyBase::get_group())
				{
					MyBase::detach();
					subject->on_observers_changed();
				}
			});
		}

		Observer&					operator=(			const Observer&		OTHER) = delete;

		Observer&					operator=(			Observer&&			other) noexcept
		{
			stop_observation();
			MyBase::operator=(std::move(other));
			return *this;
		}

		Observer&					operator=(			Swap<Observer>		other)
		{
			MyBase::operator=(Swap(static_cast<MyBase&>(*other.get())));
			return *this;
		}

	protected:	// [FUNCTIONS]
		void						observe(			MySubject&			subject)
		{
			if(!MyBase::is_member_of(subject))
			{
				subject.add_end_member(*this);
				subject.on_observers_changed();
				this->on_observe(*subject.cast());
			}
		}

		void						stop_observation()
		{
			if(MySubject* subject = MyBase::get_group())
			{
				const uint32_t SUBJECT_ID = subject->ID();
				MyBase::detach();
				subject->on_observers_changed();
				on_subject_lost(nullptr, SUBJECT_ID);
			}
		}

		bool						has_subject() const
		{
			return MyBase::is_member();
		}

		SubjectT*					get_subject()
		{
			Subject<SubjectT>* subject = MyBase::get_group();
			return subject ? subject->cast() : nullptr;
		}

		const SubjectT*				get_subject() const
		{
			const Subject<SubjectT>* SUBJECT = MyBase::get_group();
			return SUBJECT ? SUBJECT->cast() : nullptr;
		}

	private:	// [INTERFACE]
		virtual void				on_observe(			SubjectT&			subject){}

		virtual void				on_update(			SubjectT&			subject){}

		virtual void				on_subject_lost(	const SubjectT*		ALWAYS_NULL,
														const uint32_t		UNIQUE_ID){}
	};
}