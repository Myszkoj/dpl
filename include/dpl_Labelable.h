#pragma once


#include <array>
#include <string>
#include <functional>
#include <random>
#include "dpl_Archive.h"
#include "dpl_GeneralException.h"
#include "dpl_Binary.h"


// forward declarations
namespace dpl
{
	template<typename CharT>
	concept is_character = std::is_same_v<char, CharT> 
						|| std::is_same_v<wchar_t, CharT>;

	template<is_character T>
	struct	Label;

	template<is_character T = char>
	class	Labelable;

	template<is_character T = char>
	class	Labeler;
}

// implementations
namespace dpl
{
	template<>
	struct	Label<char>
	{
		using Type = std::string;
		using View = std::string_view;
	};


	template<>
	struct	Label<wchar_t>
	{
		using Type = std::wstring;
		using View = std::wstring_view;
	};


	/*
		Labels labelable objects with unique names.
	*/
	template<is_character T>
	class	Labeler : private Archive<Labelable<T>, typename Label<T>::Type>
	{
	private: // subtypes
		using	MyLabel		= typename Label<T>::Type;
		using	MyLabelable	= Labelable<T>;
		using	MyBase		= Archive<MyLabelable, MyLabel>;

	public: // constants
		static const uint32_t	MIN_CHARACTERS	= 2;
		static const uint32_t	MAX_CHARACTERS	= 256;

	public: // subtypes
		using Indexer		= std::function<uint32_t()>;
		using MyBase::find_entry;
		using MyBase::reserve;

	public: // lifecycle
		CLASS_CTOR				Labeler() = default;

		CLASS_CTOR				Labeler(				const Labeler&			OTHER) = delete;

		CLASS_CTOR				Labeler(				Labeler&&				other) noexcept = default;
			
		CLASS_DTOR				~Labeler() = default;

		Labeler&				operator=(				const Labeler&			OTHER) = delete;

		Labeler&				operator=(				Labeler&&				other) noexcept = default;

		inline Labeler&			operator=(				Swap<Labeler>&			other)
		{
			MyBase::operator=(Swap<MyBase>(*other));
			return *this;
		}

	public: // functions
		inline void				label(					MyLabelable&			labelable,
														const MyLabel&			LABEL)
		{
			if(!is_valid_numCharacters((uint32_t)LABEL.size())) throw_name_invalid(LABEL);
			if(!label_internal(labelable, LABEL))				throw_name_not_unique(LABEL);
		}

		void					label_with_postfix(		MyLabelable&			labelable,
														const MyLabel&			LABEL,
														const Indexer&			INDEXER,
														const uint32_t			MAX_RANDOM_ATTEMPTS = 10)
		{
			if(label_by_index(labelable, LABEL, INDEXER))				return;
			if(label_by_pointer(labelable, LABEL))						return;
			if(label_by_random(labelable, LABEL, MAX_RANDOM_ATTEMPTS))	return;
			throw_name_generation_failed();
		}

		inline void				label_with_postfix(		MyLabelable&			labelable,
														const MyLabel&			LABEL,
														const uint32_t			MAX_RANDOM_ATTEMPTS = 10)
		{
			return label_with_postfix(labelable, LABEL, get_default_indexer(), MAX_RANDOM_ATTEMPTS);
		}

		inline std::string		generate_indexed_label(	const MyLabel&			LABEL,
														const Indexer&			INDEXER) const
		{
			return LABEL + std::to_string(INDEXER());
		}

		inline std::string		generate_indexed_label(	const MyLabel&			LABEL) const
		{
			return LABEL + std::to_string(get_default_indexer());
		}

		inline std::string		generate_pointer_label(	const MyLabel&			LABEL,
														const void*				ADDRESS) const
		{
			return LABEL + std::to_string(uint64_t(ADDRESS));
		}

		inline std::string		generate_random_label(	const MyLabel&			LABEL) const
		{
			thread_local std::uniform_int_distribution<uint64_t> UINT_DIST;
			thread_local std::mt19937	rng;
			return LABEL + std::to_string(UINT_DIST(rng));
		}	

	private: // functions
		inline bool				is_valid_numCharacters(	const uint32_t			NUM_CHARACTERS) const
		{
			return NUM_CHARACTERS >= MIN_CHARACTERS && NUM_CHARACTERS <= MAX_CHARACTERS;
		}

		inline const Indexer	get_default_indexer()
		{
			// Delegated to a function because compiler complains about the same function name.
			return [&](){return MyBase::get_numEntries();};
		}

		inline bool				label_internal(			MyLabelable&			labelable,
														const MyLabel&			LABEL)
		{
			return MyBase::add_entry(labelable, LABEL);
		}

		inline bool				label_by_index(			MyLabelable&			labelable,
														const MyLabel&			LABEL,
														const Indexer&			INDEXER)
		{
			return label_internal(labelable, generate_indexed_label(LABEL, INDEXER));
		}

		inline bool				label_by_pointer(		MyLabelable&			labelable,
														const MyLabel&			LABEL)
		{
			return label_internal(labelable, LABEL + generate_pointer_label(LABEL, &labelable));
		}

		inline bool				label_by_random(		MyLabelable&			labelable,
														const MyLabel&			LABEL,
														uint32_t				attempts = 10)
		{
			while(attempts-- > 0)
			{
				if(label_internal(labelable, generate_random_label(LABEL)))
					return true; // success!
			}

			return false;
		}

	private: // exceptions
		inline void				throw_name_invalid(		const MyLabel&			LABEL) const
		{
			throw GeneralException(this, __LINE__, "Invalid number of characters in the given name: " + LABEL);
		}

		inline void				throw_name_not_unique(	const MyLabel&			LABEL) const
		{
			throw GeneralException(this, __LINE__, "Given name is already taken: " + LABEL);
		}

		inline void				throw_name_generation_failed() const
		{
			throw GeneralException(this, __LINE__, "Cannot generate unique name.");
		}
	};


	/*
		Interface for uniquely named objects.
	*/
	template<is_character T>
	class	Labelable : public Entry<Labelable<T>, typename Label<T>::Type>
	{
	private: // subtypes
		using	MyLabel		= typename Label<T>::Type;
		using	MyLabeler	= Labeler<T>;
		using	MyEntryType	= Entry<Labelable<T>, MyLabel>;

	public: // relations
		friend	MyLabeler;

	public: // lifecycle
		CLASS_CTOR					Labelable() = default;

		CLASS_CTOR					Labelable(					const Labelable&		OTHER) = delete;

		CLASS_CTOR					Labelable(					Labelable&&				other) noexcept = default;

		CLASS_DTOR					~Labelable() = default;

		Labelable&					operator=(					const Labelable&		OTHER) = delete;

		Labelable&					operator=(					Labelable&&				other) noexcept = default;

		Labelable&					operator=(					Swap<Labelable>			other)
		{
			MyEntryType::operator=(Swap<MyEntryType>(*other));
			return *this;
		}

	public: // functions
		inline bool					has_label() const
		{
			return MyEntryType::other() != nullptr;
		}

		const MyLabel&				get_label() const
		{
			if(MyEntryType::archive()) return MyEntryType::get_key_value();
			if constexpr (std::is_same_v<MyLabel, typename Label<char>::Type>)
			{
				static const std::string MISSING = "??text_missing??";
				return MISSING;
			}
			else
			{
				static const std::wstring MISSING = L"??text_missing??";
				return MISSING;
			}
		}

	protected: // functions
		inline MyLabeler*			get_labeler()
		{
			return static_cast<MyLabeler*>(MyEntryType::archive());
		}

		inline const MyLabeler*		get_labeler() const
		{
			return static_cast<const MyLabeler*>(MyEntryType::archive());
		}

		inline bool					change_label(				const MyLabel&			NEW_NAME)
		{
			return NEW_NAME.size() > 0 ? MyEntryType::change_key_value(NEW_NAME) : false;
		}

		bool						change_to_generic_label(	const MyLabel&			GENERIC_NAME)
		{
			if(MyLabeler* labeler = get_labeler())
			{
				if(!GENERIC_NAME.empty())
				{
					if(MyEntryType::change_key_value(labeler->generate_indexed_label(GENERIC_NAME)))		return true;
					if(MyEntryType::change_key_value(labeler->generate_pointer_label(GENERIC_NAME, this)))	return true;
					if(MyEntryType::change_key_value(labeler->generate_random_label(GENERIC_NAME)))			return true;
				}
			}

			return false;
		}

	protected: // import/export
		bool						import_label(				std::istream&			binary)
		{
			MyLabel newName;
			dpl::import_dynamic_container(binary, newName);
			return change_label(newName);
		}

		bool						export_label(				std::ostream&			binary) const
		{
			static const MyLabel NO_NAME;
			dpl::export_container(binary, has_label()? get_label() : NO_NAME);
			return has_label();
		}
	};
}