#pragma once


#include <unordered_map>
#include "dpl_Subject.h"
#include "dpl_GeneralException.h"

// forward declarations
namespace dpl
{
	template<typename SubjectT>
	class Reference;

	template<typename SubjectT>
	class Compendium;
}

// implementations
namespace dpl
{
	/*
		Reference to the Subject.
	*/
	template<typename SubjectT>
	class Reference	: private Observer<SubjectT>
					, public Member<Compendium<SubjectT>, Reference<SubjectT>>
	{
	private: // subtypes
		using	MyType			= Reference<SubjectT>;
		using	MySubject		= Subject<SubjectT>;
		using	MyCompendium	= Compendium<SubjectT>;
		using	MyBase			= Observer<SubjectT>;
		using	MySource		= Member<MyCompendium, MyType>;

	public: // relations
		friend	MyCompendium;
		friend	MyBase;

	public: // lifecycle
		CLASS_CTOR			Reference(		MySubject&				subject,
											MyCompendium&			compendium)
			: MySource(compendium)
		{
			MyBase::observe(subject);
		}

		CLASS_CTOR			Reference(		const Reference&		OTHER) = delete;

		CLASS_CTOR			Reference(		Reference&&				other) noexcept
			: MyBase(std::move(other))
			, MySource(std::move(other))
		{
			
		}

		CLASS_DTOR			~Reference() = default;

		inline Reference&	operator=(		const Reference&		OTHER) = delete;

		inline Reference&	operator=(		Reference&&				other) noexcept = default;

		inline Reference&	operator=(		Swap<Reference>			other)
		{
			MyBase::operator=(Swap<MyBase>(*other.get()));
			MySource::operator=(Swap(*other));
			return *this;
		}

	private: // interface
		virtual void		on_observe(		SubjectT&				subject) final override
		{
			// do nothing
		}

		virtual void		on_update(		SubjectT&				subject) final override
		{
			validate_compendium();
			MySource::get_chain()->on_subject_updated(*MyBase::get_subject());
		}

		virtual void		on_subject_lost(const SubjectT*			DUMMY,
											const uint32_t			UNIQUE_ID) final override
		{
			validate_compendium();
			MySource::get_chain()->remove_subject_internal(UNIQUE_ID); //<-- Effectively selfdestruct
		}

	private: // functions
		inline void			validate_compendium() const
		{
#ifdef _DEBUG
			if(!MySource::is_linked())
				throw GeneralException(this, __LINE__, "Compendium is missing.");
#endif // _DEBUG
		}
	};


	/*
		Stores description for each observed subject.
	*/
	template<typename SubjectT>
	class Compendium : public Group<Compendium<SubjectT>, Reference<SubjectT>>
	{
	public: // subtypes
		using Callback		= std::function<void(SubjectT&)>;
		using ConstCallback = std::function<void(const SubjectT&)>;

	private: // subtypes
		using MySubject		= Subject<SubjectT>;
		using MyReference	= Reference<SubjectT>;
		using MyReferences	= std::unordered_map<uint32_t, MyReference>;
		using MyOrder		= Group<Compendium<SubjectT>, Reference<SubjectT>>;

	public: // relations
		friend MyReference;

	private: // data
		MyReferences m_references;

	protected: // lifecycle
		CLASS_CTOR					Compendium() = default;

		CLASS_CTOR					Compendium(				const Compendium&	OTHER) = delete;
			
		CLASS_CTOR					Compendium(				Compendium&&		other) noexcept
			: MyOrder(std::move(other))
			, m_references(std::move(other.m_references))
		{
			
		}
			
		CLASS_DTOR					~Compendium() = default;

		inline Compendium&			operator=(				const Compendium&	OTHER) = delete;

		inline Compendium&			operator=(				Compendium&&		other) noexcept
		{		
			MyOrder::operator=(std::move(other));
			m_references = std::move(other.m_references);
			return *this;
		}

		inline Compendium&			operator=(				Swap<Compendium>	other)
		{
			MyOrder::operator=(Swap<MyOrder>(*other));
			std::swap(m_references, other->m_references);
			return *this;
		}

	protected: // functions
		inline uint32_t				size() const
		{
			return MyOrder::size();
		}

		inline bool					add_subject(			MySubject&			subject)
		{
			return m_references.try_emplace(subject.ID(), subject, *this).second;
		}

		inline void					for_each_subject(		Callback			function)
		{
			MyOrder::for_each_link([&](Reference<SubjectT>& reference)
			{
				function(*reference.get_subject());
			});
		}

		inline void					for_each_subject(		ConstCallback		FUNCTION) const
		{
			MyOrder::for_each_link([&](const Reference<SubjectT>& REFERENCE)
			{
				FUNCTION(*REFERENCE.get_subject());
			});
		}

		inline bool					remove_subject(			const MySubject&	SUBJECT)
		{
			return remove_subject_internal(SUBJECT.ID());
		}

		inline void					remove_all_subjects()
		{
			m_references.clear();
		}

	private: // functions
		inline bool					remove_subject_internal(const uint32_t		SUBJECT_ID)
		{
			return m_references.erase(SUBJECT_ID) > 0;
		}

	private: // interface
		virtual void				on_subject_updated(		SubjectT&			subject){}
	};
}