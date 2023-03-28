#pragma once


#include <array>
#include <string>
#include <functional>
#include <random>
#include "dpl_Archive.h"
#include "dpl_GeneralException.h"
#include "dpl_Binary.h"


namespace dpl
{
//============ DECLARATIONS ============//
	template<typename CharT = char>
	class	Labelable;

	template<typename CharT = char>
	class	Labeler;

//============ DEFINITIONS ============//

	template<typename CharT>
	struct Label;

	template<>
	struct Label<char>
	{
		using Type = std::string;
		using View = std::string_view;
	};


	template<>
	struct Label<wchar_t>
	{
		using Type = std::wstring;
		using View = std::wstring_view;
	};


	/*
		Labels labelable objects with unique names.
	*/
	template<typename CharT>
	class Labeler : private Archive<Labelable<CharT>, typename Label<CharT>::Type>
	{
	private: // subtypes
		using MyLabel		= typename Label<CharT>::Type;
		using MyLabelable	= Labelable<CharT>;
		using MyBase		= Archive<MyLabelable, MyLabel>;

	public: // constants
		static const uint32_t	MIN_CHARACTERS	= 2;
		static const uint32_t	MAX_CHARACTERS	= 256;

	public: // subtypes
		using Indexer		= std::function<uint32_t()>;
		using MyBase::find_entry;

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
			static const std::uniform_int_distribution<uint64_t> UINT_DIST;
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
			return label_internal(labelable, LABEL + generate_pointer_label(&labelable));
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
	template<typename CharT>
	class Labelable : public Entry<Labelable<CharT>, typename Label<CharT>::Type>
	{
	private: // subtypes
		using MyLabel		= typename Label<CharT>::Type;
		using MyLabeler		= Labeler<CharT>;
		using MyEntryType	= Entry<Labelable<CharT>, MyLabel>;

	public: // relations
		friend MyLabeler;

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


namespace dpl_experimental
{
	using namespace dpl;

//============ DECLARATIONS ============//
	template<uint32_t NUM_BASE_CHARACTERS = 16, typename CharT = char>
	class	Label;

	template<uint32_t NUM_BASE_CHARACTERS = 16, typename CharT = char>
	class	Labelable;

	template<uint32_t NUM_BASE_CHARACTERS = 16, typename CharT = char>
	class	Labeler;

//============ DEFINITIONS ============//

	template<uint32_t NUM_BASE_CHARACTERS, typename CharT>
	class	Label
	{
	public: // relations
		friend Labeler<NUM_BASE_CHARACTERS, CharT>;
		friend Labelable<NUM_BASE_CHARACTERS, CharT>;

	public: // constants
		static const uint32_t	MAX_INDEX_CHARACTERS	= 20;
		static const uint32_t	MAX_POSTFIX_CHARACTERS	= MAX_INDEX_CHARACTERS + 1; // + '_'
		static const uint32_t	BUFFER_SIZE				= NUM_BASE_CHARACTERS + MAX_POSTFIX_CHARACTERS + 1; // + '\0'
		static const uint32_t	MIN_BASE_CHARACTERS		= 2;
		static const uint32_t	MAX_BASE_CHARACTERS		= 2048;
		static const bool		IS_CHAR					= std::is_same<char, CharT>::value;
		static const bool		IS_WCHAR				= std::is_same<wchar_t, CharT>::value;

		static_assert(NUM_BASE_CHARACTERS >= MIN_BASE_CHARACTERS, "Not enough basic characters. Min = 2");
		static_assert(NUM_BASE_CHARACTERS < MAX_BASE_CHARACTERS, "Too many base characters. Max = 2048");
		static_assert(IS_CHAR || IS_WCHAR, "Label CharT must be either char or wchar_t.");

	public: // subtypes
		using Buffer	= std::array<CharT, BUFFER_SIZE>;
		using View		= std::conditional<IS_CHAR, std::string_view, std::wstring_view>::type;
		using String	= std::conditional<IS_CHAR, std::string, std::wstring>::type;

	private: // data
		Buffer		m_buffer;
		uint32_t	m_size; // Excludes '\0' character.

	private: // lifecycle
		CLASS_CTOR			Label()
			: m_size(0)
		{
			set_end();
		}

		CLASS_CTOR			Label(					const View		STRING_VIEW)
			: Label()
		{
			set(STRING_VIEW);
		}

		CLASS_CTOR			Label(					const View		STRING_VIEW,
													const uint64_t	INDEX)
			: Label(STRING_VIEW)
		{
			CharT indexBuffer[MAX_INDEX_CHARACTERS+1]; // can hold 2^32 - 1, plus NUL
			CharT* const END = &indexBuffer[MAX_INDEX_CHARACTERS];
			CharT* _postfix = std::_UIntegral_to_buff(END, INDEX);
				*--_postfix	= static_cast<CharT>('_');
			const uint32_t POSTFIX_SIZE = (uint32_t)(END-_postfix);
			memcpy(m_buffer.data()+m_size, _postfix, POSTFIX_SIZE * sizeof(CharT));
			m_size += POSTFIX_SIZE;
			set_end();
		}

	public: // lifecycle
		CLASS_CTOR			Label(					const Label&	OTHER)
			: Label(OTHER.m_buffer.data(), OTHER.m_buffer.size())
		{

		}

		CLASS_CTOR			Label(					Label&&			other) noexcept
			: Label(other.m_buffer.data(), other.m_buffer.size())
		{
			other.m_size = 0;
			other.set_end();
		}

		CLASS_DTOR			~Label() = default;

	public: // operators
		inline Label&		operator=(				const Label&	OTHER)
		{
			set(OTHER.m_buffer.data(), OTHER.m_buffer.size());
			return *this;
		}

		inline Label&		operator=(				Label&			other) noexcept
		{
			set(other.m_buffer.data(), other.m_buffer.size());
			other.m_size = 0;
			other.set_end();
			return *this;
		}

		inline bool			operator==(				const Label&	OTHER) const
		{ 
			return strcmp(m_buffer.data(), OTHER.m_buffer.data()) == 0;
		}

		inline operator		View() const
		{
			return view();
		}

		inline Label&		operator<<(				std::istream&	binary)
		{
			dpl::import_static_container(binary, m_buffer);
			return *this;
		}

		inline const Label&	operator>>(				std::ostream&	binary) const
		{
			dpl::export_container(binary, m_buffer);
			return *this;
		}

	public: // functions
		inline View			view() const
		{
			return View(m_buffer.data(), m_size+1);
		}

		inline String		str() const
		{
			return String(view());
		}

		inline uint32_t		size() const
		{
			return m_size;
		}

		inline size_t		hash() const
		{
			return std::hash<View>()(*this);
		}

		static bool			is_valid_numCharacters(	const size_t	NUM_CHARACTERS)
		{
			if(NUM_CHARACTERS < MIN_BASE_CHARACTERS) return false;
			if(NUM_CHARACTERS > NUM_BASE_CHARACTERS) return false;
			return true;
		}

	private: // functions
		inline void			validate_base_size(		const size_t	NEW_SIZE) const
		{
			if(NEW_SIZE < MIN_BASE_CHARACTERS) throw GeneralException(this, __LINE__, "Not enough characters.");
			if(NEW_SIZE < NUM_BASE_CHARACTERS) return;
			throw GeneralException(this, __LINE__, "Too many characters.");
		}

		inline void			set_end()
		{
			m_buffer[m_size] = '\0';
		}

		inline void			set(					const View		STRING_VIEW)
		{
			memcpy(m_buffer.data(), STRING_VIEW.data(), STRING_VIEW.size() * sizeof(CharT));
			m_size = (uint32_t)STRING_VIEW.size();
			set_end();
		}

		inline void			set_base(				const View		STRING_VIEW)
		{
			validate_base_size(STRING_VIEW.size());
			set(STRING_VIEW);
		}
	};
}

namespace std 
{
	template <uint32_t NUM_BASE_CHARACTERS, typename CharT>
	struct hash<dpl_experimental::Label<NUM_BASE_CHARACTERS, CharT>>
	{
		inline size_t operator()(const dpl_experimental::Label<NUM_BASE_CHARACTERS, CharT>& LABEL) const
		{
			return LABEL.hash();
		}
	};
}

namespace dpl_experimental
{
	using namespace dpl;

	template<uint32_t NUM_BASE_CHARACTERS, typename CharT>
	class	Labelable : public Entry<Labelable<NUM_BASE_CHARACTERS, CharT>, Label<NUM_BASE_CHARACTERS, CharT>>
	{
	private: // subtypes
		using MyType		= Labelable<NUM_BASE_CHARACTERS, CharT>;
		using MyLabel		= Label<NUM_BASE_CHARACTERS, CharT>;
		using MyLabeler		= Labeler<NUM_BASE_CHARACTERS, CharT>;
		using MyEntryType	= Entry<MyType, MyLabel>;
		using Buffer		= typename MyLabel::Buffer;
		using Characters	= typename MyLabel::View;

	public: // relations
		friend MyLabeler;

	protected: // lifecycle
		CLASS_CTOR				Labelable() = default;

		CLASS_CTOR				Labelable(		const Labelable&	OTHER) = delete;

		CLASS_CTOR				Labelable(		Labelable&&			other) noexcept = default;

		CLASS_DTOR				~Labelable() = default;

		Labelable&				operator=(		const Labelable&	OTHER) = delete;

		Labelable&				operator=(		Labelable&&			other) noexcept = default;

		inline Labelable&		operator=(		Swap<Labelable>		other)
		{
			return static_cast<Labelable&>(MyEntryType::operator=(Swap<MyEntryType>(*other)));
		}

	public: // functions
		inline bool				has_label() const
		{
			return MyEntryType::other() != nullptr;
		}

		inline const MyLabel&	get_label() const
		{
			static MyLabel INVALID_LABEL;
			return MyEntryType::archive()? MyEntryType::get_key_value() : INVALID_LABEL;
		}

	protected: // functions
		inline bool				change_label(	const Characters	NEW_CHARACTERS)
		{
			return MyLabel::is_valid_numCharacters(NEW_CHARACTERS.size()) ? MyEntryType::change_key_value(NEW_CHARACTERS) : false;
		}

		inline MyLabeler*		get_labeler()
		{
			return static_cast<MyLabeler*>(MyEntryType::archive());
		}

		inline const MyLabeler*	get_labeler() const
		{
			return static_cast<const MyLabeler*>(MyEntryType::archive());
		}

	protected: // import/export
		inline bool				import_label(	std::istream&		binary)
		{
			MyLabel label; label << binary;
			return change_label(label);
		}

		inline bool				export_label(	std::ostream&		binary) const
		{
			get_label() >> binary;
			return has_label();
		}
	};


	template<uint32_t NUM_BASE_CHARACTERS, typename CharT>
	class	Labeler : public Archive<Labelable<NUM_BASE_CHARACTERS, CharT>, Label<NUM_BASE_CHARACTERS, CharT>>
	{
	private: // subtypes
		using MyLabelable	= Labelable<NUM_BASE_CHARACTERS, CharT>;
		using MyLabel		= Label<NUM_BASE_CHARACTERS, CharT>;
		using MyBase		= Archive<MyLabelable, MyLabel>;

	public: // subtypes
		using Indexer		= std::function<uint64_t()>;
		using StringView	= typename MyLabel::View;

	public: // lifecycle
		CLASS_CTOR				Labeler() = default;

		CLASS_CTOR				Labeler(			const Labeler&			OTHER) = delete;

		CLASS_CTOR				Labeler(			Labeler&&				other) noexcept = default;
			
		CLASS_DTOR				~Labeler() = default;

		Labeler&				operator=(			const Labeler&			OTHER) = delete;

		Labeler&				operator=(			Labeler&&				other) noexcept = default;

		inline Labeler&			operator=(			Swap<Labeler>			other)
		{
			return static_cast<Labeler&>(MyBase::operator=(Swap<MyBase>(*other)));
		}

	public: // functions
		inline bool				label(				MyLabelable&			labelable,
													const StringView		CHARACTERS)
		{
			if(!MyLabel::is_valid_numCharacters(CHARACTERS.size())) return false;
			return label_internal(MyLabel(CHARACTERS), labelable);
		}

		void					label_with_postfix(	MyLabelable&			labelable,
													const StringView		CHARACTERS,
													const Indexer			INDEXER,
													const uint32_t			MAX_RANDOM_ATTEMPTS = 10)
		{
			if(label_by_index(labelable, CHARACTERS, INDEXER))				return;
			if(label_by_pointer(labelable, CHARACTERS))						return;
			if(label_by_random(labelable, CHARACTERS, MAX_RANDOM_ATTEMPTS)) return;
			throw GeneralException(this, __LINE__, "Cannot generate unique name.");
		}

		inline void				label_with_postfix(	MyLabelable&			labelable,
													const StringView		CHARACTERS,
													const uint32_t			MAX_RANDOM_ATTEMPTS = 10)
		{
			return label_with_postfix(labelable, CHARACTERS, get_default_indexer(), MAX_RANDOM_ATTEMPTS);
		}

	private: // functions
		inline const Indexer	get_default_indexer()
		{
			// Delegated to a function because compiler complains about the same function name.
			return [&](){return MyBase::get_numEntries();};
		}

		inline bool				label_internal(		const MyLabel			LABEL,
													MyLabelable&			labelable)
		{
			return MyBase::add_entry(labelable, LABEL);
		}

		inline bool				label_by_index(		MyLabelable&			labelable,
													const StringView		CHARACTERS,
													const Indexer			INDEXER)
		{
			return label_internal(MyLabel(CHARACTERS, INDEXER()), labelable);
		}

		inline bool				label_by_pointer(	MyLabelable&			labelable,
													const StringView		CHARACTERS)
		{
			return label_internal(MyLabel(CHARACTERS, uint64_t(&labelable)), labelable);
		}

		inline bool				label_by_random(	MyLabelable&			labelable,
													const StringView		CHARACTERS,
													uint32_t				attempts = 10)
		{
			static const std::uniform_int_distribution<uint64_t> UINT_DIST; 
			thread_local std::mt19937	rng;

			while(attempts-- > 0)
			{
				if(label_internal(MyLabel(CHARACTERS, UINT_DIST(rng)), labelable))
					return true; // success!
			}

			return false;
		}
	};
}