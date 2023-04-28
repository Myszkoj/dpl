#pragma once


#include <unordered_map>
#include "dpl_Subject.h"
#include "dpl_GeneralException.h"

// forward declarations
namespace dpl
{
	template<typename SubjectT, typename InfoT>
	class Info;

	template<typename SubjectT, typename InfoT>
	class Compendium;
}

// implementations
namespace dpl
{
	template<typename SubjectT, typename InfoT>
	class Info		: private Observer<SubjectT>
					, private Member<Compendium<SubjectT, InfoT>, InfoT>
	{
	private:	// [SUBTYPES]
		using	MyType			= Info<SubjectT, InfoT>;
		using	MySubject		= Subject<SubjectT>;
		using	MyCompendium	= Compendium<SubjectT, InfoT>;
		using	MyReference		= Observer<SubjectT>;
		using	MySource		= Member<MyCompendium, InfoT>;

		class	Binding
		{
		public:		// [FRIENDS]
			friend Info;
			friend MyCompendium;

		private:	// [DATA]
			MySubject*		subject;
			MyCompendium*	compendium;

		private:	// [LIFECYCLE]
			CLASS_CTOR	Binding(MySubject&		subject,
								MyCompendium&	compendium)
				: subject(&subject)
				, compendium(&compendium)
			{

			}
		};

	public:		// [FRIENDS]
		friend	MyCompendium;
		friend	MyReference;
		friend	MySource;
		friend	MyReference::MyLink;
		friend	MySource::MyLink;
		friend	MySource::MyGroup;

	public:	// [LIFECYCLE]
		CLASS_CTOR			Info(				const Binding&			BINDING)
			: MySource(*BINDING.compendium)
		{
			MyReference::observe(*BINDING.subject);
		}

		CLASS_CTOR			Info(				const Info&				OTHER) = delete;

		CLASS_CTOR			Info(				Info&&					other) noexcept
			: MyReference(std::move(other))
			, MySource(std::move(other))
		{
			
		}

		CLASS_DTOR			~Info() = default;

		Info&				operator=(			const Info&				OTHER) = delete;

		Info&				operator=(			Info&&					other) noexcept = default;

		Info&				operator=(			Swap<Info>				other)
		{
			MyReference::operator=(Swap<MyReference>(*other.get()));
			MySource::operator=(Swap(*other));
			return *this;
		}

	public:		// [FUNCTIONS]
		using MyReference::has_subject;
		using MyReference::get_subject;

	private:	// [INTERFACE]
		virtual void		on_observe(			SubjectT&				subject) final override
		{
			// do nothing
		}

		virtual void		on_update(			SubjectT&				subject) final override
		{
			validate_compendium();
			MySource::get_group()->on_subject_updated(*MyReference::get_subject());
		}

		virtual void		on_subject_lost(	const SubjectT*			DUMMY,
												const uint32_t			UNIQUE_ID) final override
		{
			validate_compendium();
			MySource::get_group()->remove_subject_internal(UNIQUE_ID); //<-- Effectively selfdestruct
		}

	private:	// [DEBUG]
		void				validate_compendium() const
		{
#ifdef _DEBUG
			if(!MySource::is_member())
				throw GeneralException(this, __LINE__, "Compendium is missing.");
#endif // _DEBUG
		}
	};


	template<typename SubjectT>
	class NoInfo : public Info<SubjectT, NoInfo<SubjectT>>
	{
	private:	// [SUBTYPES]
		using MyInfoBase = Info<SubjectT, NoInfo<SubjectT>>;

	public:		// [LIFECYCLE]
		using MyInfoBase::MyInfoBase;
		using MyInfoBase::operator=;
	};


	// Stores description for each observed subject in the form of InfoT objects.
	template<typename SubjectT, typename InfoT = NoInfo<SubjectT>>
	class Compendium : private Group<Compendium<SubjectT, InfoT>, InfoT>
	{
	public:		// [SUBTYPES]
		using	InvokeInfo			= std::function<void(InfoT&)>;
		using	InvokeConstInfo		= std::function<void(const InfoT&)>;
		using	InvokeSubject		= std::function<void(SubjectT&)>;
		using	InvokeConstSubject	= std::function<void(const SubjectT&)>;

	private:	// [SUBTYPES]
		using	MyType				= Compendium<SubjectT, InfoT>;
		using	MyBase				= Group<MyType, InfoT>;
		using	MySequence			= Sequence<InfoT>;
		using	MySubject			= Subject<SubjectT>;
		using	MyInfo				= Info<SubjectT, InfoT>;
		using	MyCollection		= std::unordered_map<uint32_t, InfoT>;
		using	Binding				= typename MyInfo::Binding;

	public:		// [FRIENDS]
		friend	MyBase;
		friend	MySequence;
		friend	MyInfo;

	private:	// [DATA]
		MyCollection m_collection;

	protected:	// [LIFECYCLE]
		CLASS_CTOR					Compendium() = default;

		CLASS_CTOR					Compendium(				const Compendium&			OTHER) = delete;
			
		CLASS_CTOR					Compendium(				Compendium&&				other) noexcept
			: MyBase(std::move(other))
			, m_collection(std::move(other.m_collection))
		{
			
		}
			
		CLASS_DTOR					~Compendium() = default;

		Compendium&					operator=(				const Compendium&			OTHER) = delete;

		Compendium&					operator=(				Compendium&&				other) noexcept
		{		
			MyBase::operator=(std::move(other));
			m_collection = std::move(other.m_collection);
			return *this;
		}

		Compendium&					operator=(				Swap<Compendium>			other)
		{
			MyBase::operator=(Swap<MyBase>(*other));
			std::swap(m_collection, other->m_collection);
			return *this;
		}

	public:		// [FUNCTIONS]
		using MyBase::size;

		void						for_each_info(			const InvokeInfo&			INVOKE)
		{
			MyBase::for_each_member(INVOKE);
		}

		void						for_each_info(			const InvokeConstInfo&		INVOKE) const
		{
			MyBase::for_each_member(INVOKE);
		}

		void						for_each_subject(		const InvokeSubject&		INVOKE)
		{
			Compendium::for_each_info([&](InfoT& info)
			{
				INVOKE(*info.get_subject());
			});
		}

		void						for_each_subject(		const InvokeConstSubject&	INVOKE) const
		{
			Compendium::for_each_info([&](const InfoT& INFO)
			{
				INVOKE(*INFO.get_subject());
			});
		}

	protected:	// [FUNCTIONS]
		template<typename... CTOR>
		bool						add_subject(			MySubject&					subject,
															CTOR&&...					args)
		{
			return m_collection.try_emplace(subject.ID(), Binding(subject, *this), std::forward<CTOR>(args)...).second;
		}

		bool						remove_subject(			const MySubject&			SUBJECT)
		{
			return remove_subject_internal(SUBJECT.ID());
		}

		void						remove_all_subjects()
		{
			m_collection.clear();
		}

	private:	// [FUNCTIONS]
		bool						remove_subject_internal(const uint32_t				SUBJECT_ID)
		{
			return m_collection.erase(SUBJECT_ID) > 0;
		}

	private:	// [INTERFACE]
		virtual void				on_subject_updated(		SubjectT&					subject){}
	};
}