#pragma once


#include "dpl_Association.h"
#include "dpl_ReadOnly.h"
#include <unordered_set>
#include <xhash>
#include <functional>


// forward declarations
namespace dpl
{
	template<typename EntryT, typename KeyValueT>
	class Archive;

	template<typename EntryT, typename KeyValueT>
	class Key;

	template<typename EntryT, typename KeyValueT>
	class Entry;
}

// definitions
namespace dpl
{
	static constexpr uint32_t KEY_ENTRY_ASSOCIATION_ID = 11111111;

	/*
		Provides a means of gaining access to archive entries.
	*/
	template<typename EntryT, typename KeyValueT>
	class Key : public Association<Key<EntryT, KeyValueT>, Entry<EntryT, KeyValueT>, KEY_ENTRY_ASSOCIATION_ID>
	{
	public: // relations
		friend Archive<EntryT, KeyValueT>;
		friend Entry<EntryT, KeyValueT>;

	public: // subtypes
		using ValueT = KeyValueT;

	public: // data
		ReadOnly<ValueT, Key> value; // Any type that supports hashing.

	public: // lifecycle
		CLASS_CTOR		Key(		const ValueT&		VALUE)
			: value(VALUE)
		{

		}

	public: // operators
		inline bool		operator==(	const Key&			OTHER) const
		{
			return value() == OTHER.value();
		}

		inline bool		operator!=(	const Key&			OTHER) const
		{
			return value() != OTHER.value();
		}
	};



	/*
		 Base class for the archive entries.
	*/
	template<typename EntryT, typename KeyValueT>
	class Entry : public Association<Entry<EntryT, KeyValueT>, Key<EntryT, KeyValueT>, KEY_ENTRY_ASSOCIATION_ID>
	{
	public: // relations
		friend Archive<EntryT, KeyValueT>;
		friend Key<EntryT, KeyValueT>;

	private: // subtypes
		using MyArchive = Archive<EntryT, KeyValueT>;
		using MyKey		= Association<Entry<EntryT, KeyValueT>, Key<EntryT, KeyValueT>, KEY_ENTRY_ASSOCIATION_ID>;

	public: // data
		mutable ReadOnly<MyArchive*, Entry> archive;

	protected: // lifecycle
		CLASS_CTOR					Entry()
			: archive(nullptr)
		{

		}

		CLASS_CTOR					Entry(				const Entry&		OTHER) = delete;

		CLASS_CTOR					Entry(				Entry&&				other) noexcept
			: MyKey(std::move(other))
			, archive(other.archive)
		{
			other.archive = nullptr;
		}
			
		CLASS_DTOR					~Entry()
		{
			dpl::no_except([&]() {	extract();	});
		}

		Entry&						operator=(			const Entry&		OTHER) = delete;

		Entry&						operator=(			Entry&&				other) noexcept
		{
			extract();
			MyKey::operator=(std::move(other));
			archive			= other.archive;
			other.archive	= nullptr;
			return *this;
		}

		inline Entry&				operator=(			Swap<Entry>			other)
		{
			MyKey::operator=(Swap<Entry<EntryT, KeyValueT>>(*other));
			std::swap(*archive, *other->archive);
			return *this;
		}

	public: // functions
		/*
			Returns reference to the value stored by the key.
			Note: Make sure that entry is part of the archive.
		*/
		inline const KeyValueT&		get_key_value() const
		{
			return MyKey::other()->value();
		}

		inline bool					change_key_value(	const KeyValueT&	KEY_VALUE)
		{
			return archive() ? archive()->change_key_value(*this, KEY_VALUE) : false;
		}

	private: // functions
		inline void					extract() noexcept
		{
			if(archive()) try{archive()->remove_entry(*this);}catch(...){}
		}

		inline void					update_archive(		MyArchive*			newArchive) const
		{
			archive = newArchive;
		}
	};



	/*
		Stores references to the entries within key objects.

		TODO: In full version of c++20 there is an std::unordered_set::find overload that accepts any value.
		In that case, key value may be stored in the Entry class.
	*/
	template<typename EntryT, typename KeyValueT>
	class Archive
	{
	private: // subtypes
		using MyKey			= Key<EntryT, KeyValueT>;
		using MyEntry		= Entry<EntryT, KeyValueT>;
		using MyEntries		= std::unordered_set<MyKey>;

	public: // subtypes
		using Invoke		= std::function<void(EntryT&)>;
		using InvokeConst	= std::function<void(const EntryT&)>;

	private: // data
		ReadOnly<MyEntries, Archive> entries;

	public: // lifecycle
		CLASS_CTOR				Archive() = default;

		CLASS_CTOR				Archive(			const Archive&			OTHER) = delete;

		CLASS_CTOR				Archive(			Archive&&				other) noexcept
			: entries(std::move(other.entries))
		{
			notify_moved();
		}
			
		CLASS_DTOR				~Archive()
		{
			dpl::no_except([&]() {	remove_all_entries();	});
		}

		Archive&				operator=(			const Archive&			OTHER) = delete;

		Archive&				operator=(			Archive&&				other) noexcept
		{
			remove_all_entries();
			entries = std::move(other.entries);
			notify_moved();
			return *this;
		}

		Archive&				operator=(			Swap<Archive>			other)
		{
			std::swap(*entries, *other->entries);
			notify_moved();
			other->notify_moved();
			return *this;
		}

	public: // functions
		inline void				reserve(			const uint32_t			NUM_ENTRIES)
		{
			entries->reserve(NUM_ENTRIES);
		}

		inline uint32_t			get_numEntries() const
		{
			return static_cast<uint32_t>(entries().size());
		}

		/*
			Returns true if entry was successfully added to the archive, or it was already a part of it.
		*/
		bool					add_entry(			MyEntry&				entry,
													const KeyValueT&		KEY_VALUE)
		{
			if(entry.archive() != this)
			{
				entry.extract();

				auto result = entries->emplace(MyKey(KEY_VALUE));
				if(result.second)
				{
					entry.update_archive(this);
					return entry.link(const_cast<MyKey&>(*result.first));
				}

				return false;
			}

			return true;
		}

		bool					remove_entry(		MyEntry&				entry)
		{
			if(entry.archive() == this)
			{
				entry.update_archive(nullptr);
				return entries->erase(*entry.other()) > 0;
			}

			return false;
		}

		/*
			Returns true if at least one entry was removed.
		*/
		bool					remove_all_entries()
		{
			if(entries().size() > 0)
			{
				for(auto& key : *entries)
				{
					const MyEntry*	ENTRY = key.other();
									ENTRY->update_archive(nullptr);
				}

				entries->clear();
				return true;
			}

			return false;
		}

		bool					change_key_value(	MyEntry&				entry,
													const KeyValueT&		KEY_VALUE)
		{
			if(entry.archive() == this)
			{
				auto result = entries->emplace(MyKey(KEY_VALUE));
				if(result.second)
				{
					if(auto* key = entry.other())
					{
						entries->erase(*key);
					}

					return entry.link(const_cast<MyKey&>(*result.first));
				}
			}

			return false;
		}

		inline EntryT*			find_entry(			const KeyValueT&		KEY_VALUE)
		{
			return const_cast<EntryT*>(find_internal(KEY_VALUE));
		}

		inline const EntryT*	find_entry(			const KeyValueT&		KEY_VALUE) const
		{
			return find_internal(KEY_VALUE);
		}

		inline void				for_each_entry(		const Invoke&			INVOKE)
		{
			for(auto& key : *entries)
			{		
				INVOKE(static_cast<EntryT&>(*key.target()));
			}
		}

		inline void				for_each_entry(		const InvokeConst&		INVOKE) const
		{
			for(const auto& KEY : entries())
			{		
				INVOKE(static_cast<const EntryT&>(*KEY.target()));
			}
		}

	private: // functions
		inline const EntryT*	find_internal(		const KeyValueT&		KEY_VALUE) const
		{
			auto it = entries().find(MyKey(KEY_VALUE));
			return (it != entries().end()) ? static_cast<const EntryT*>(it->other()) : nullptr;
		}

		inline void				notify_moved()
		{
			for(auto& key : *entries)
			{
				const MyEntry*	ENTRY = key.target();
								ENTRY->update_archive(this);
			}
		}
	};
}

// specializations
namespace std
{
	template<typename EntryT, typename KeyValueT>
	struct hash<dpl::Key<EntryT, KeyValueT>>
	{
		size_t	operator()(	const dpl::Key<EntryT, KeyValueT>& KEY) const
		{
			return hash<KeyValueT>()(KEY.value());
		}
	};
}