#pragma once


#include <atomic>
#include "dpl_DynamicArray.h"
#include "dpl_Membership.h"
#include "dpl_Mask.h"
#include "dpl_Range.h"


#pragma pack(push, 4)

// declarations
namespace dpl
{
	template<uint32_t N, typename T>
	concept is_divisible_by	= sizeof(T)%N == 0;

	template<typename T>
	concept is_Streamable	=  std::is_trivially_destructible_v<T>
							&& std::is_copy_assignable_v<T>
							&& is_divisible_by<4, T>;

	template<is_Streamable T>
	class	StreamChunk;

	template<is_Streamable T>
	class	StreamController;
}

// definitions
namespace dpl
{
	enum	TransferFlags
	{
		RESIZED,
		NEEDS_FLUSH,
		KEPT // Inherited from transfer.
	};


	template<is_Streamable T>
	class	StreamChunk	: private dpl::Member<StreamController<T>, StreamChunk<T>>
	{
	private: // subtypes
		using	MyType			= StreamChunk<T>;
		using	MyStream		= StreamController<T>;
		using	MyMemberBase	= dpl::Member<MyStream, MyType>;
		using	MyGroup			= dpl::Group<MyStream, MyType>;

	public: // friends
		friend	MyStream;
		friend	MyMemberBase;
		friend	MyGroup;
		friend	dpl::Sequenceable<MyType>;

	public: // subtypes
		using	OnModify		= std::function<void(dpl::Buffer<T>&)>;
		using	Range			= dpl::IndexRange<uint32_t>;
		using	Invocation		= typename dpl::DynamicArray<T>::Invocation;
		using	ConstInvocation	= typename dpl::DynamicArray<T>::ConstInvocation;

	public: // data
		mutable dpl::ReadOnly<Range,	StreamChunk>	range; // Note: May be invalid if transfer was not yet updated.

	private: // data
		mutable dpl::DynamicArray<T>					container;
		mutable dpl::Mask32_t							flags;	

	public: // lifecycle
		CLASS_CTOR					StreamChunk()
		{
			flags.set_at(KEPT, true);
		}

		CLASS_CTOR					StreamChunk(					const StreamChunk&		OTHER)
			: range(0, OTHER.size())
			, flags(OTHER.flags)
		{
			OTHER.read();
			container = OTHER.container;
			if (OTHER.is_stream_ready())
			{
				const_cast<MyStream&>(OTHER.get_stream()).add_chunk(*this);
			}
		}

		CLASS_CTOR					StreamChunk(					StreamChunk&&			other) noexcept
			: MyMemberBase(std::move(other))
			, range(other.range)
			, container(std::move(other.container))
			, flags(other.flags)
		{
			other.range->reset();
		}

		CLASS_DTOR					~StreamChunk()
		{
			dpl::no_except([&](){	detach_from_stream();	});
		}

		StreamChunk&				operator=(						const StreamChunk&		OTHER)
		{
			detach_from_stream();
			OTHER.read();
			range->reset(0, OTHER.size());
			container	= OTHER.container;
			flags		= OTHER.flags;
			if (OTHER.is_stream_ready())
			{
				const_cast<MyStream&>(OTHER.get_stream()).add_chunk(*this);
			}
			return *this;
		}

		StreamChunk&				operator=(						StreamChunk&&			other) noexcept
		{
			MyMemberBase::operator=(std::move(other));
			range.swap(other.range);
			container.swap(other.container);
			std::swap(flags, other.flags);
			return *this;
		}

	public: // transfer functions
		inline bool					is_stream_ready() const
		{
			return MyMemberBase::is_member();
		}

		inline MyStream&			get_stream()
		{
			return *MyMemberBase::get_group();
		}

		inline const MyStream&		get_stream() const
		{
			return *MyMemberBase::get_group();
		}

		void						detach_from_stream()
		{
			if(restore())
			{
				mark_as_resized();
				MyMemberBase::detach();
				flags.set_at(RESIZED,		false);
				flags.set_at(NEEDS_FLUSH,	false);
				flags.set_at(KEPT,			true);
			}
		}

	public: // functions
		inline uint32_t				offset() const
		{
			return range().begin();
		}

		inline uint32_t				size() const
		{
			return range().size();
		}

		inline uint32_t				capacity() const
		{
			return container.capacity();
		}

		/*
			Modify returned array.
			Warning! Returned pointer may be invalidated when transfer is updated or array is resized.
		*/
		inline T*					modify()
		{
			restore();
			mark_as_modified();
			return container.data();
		}

		inline void					modify_each(					const Invocation&		INVOKE)
		{
			modify();
			container.for_each(INVOKE);
		}

		/*
			Get read-only data.
			Warning! Returned pointer may be invalidated when transfer is updated or array is resized.
		*/
		inline const T*				read() const
		{
			restore();
			return container.data();
		}

		inline void					read_each(						const ConstInvocation&	INVOKE) const
		{
			read();
			container.for_each(INVOKE);
		}

		/*
			Returns INVALID_INDEX if out of range.
			WARNING! This function calls read.
		*/
		inline uint32_t				index_of(						const T*				ADDRESS) const
		{
			read();
			return container.index_of(ADDRESS);
		}

		template<typename... CTOR>
		T&							emplace_back(					CTOR&&...				args)
		{
			restore();
			T& newData = container.emplace_back(std::forward<CTOR>(args)...);
			mark_as_modified();
			mark_as_resized();
			return newData;
		}

		T*							enlarge(						const uint32_t			AMOUNT)
		{
			restore();
			T* newData = container.enlarge(AMOUNT);
			mark_as_modified();
			mark_as_resized();
			return newData;
		}

		void						reduce(							const uint32_t			AMOUNT)
		{
			restore();
			container.reduce(AMOUNT);
			mark_as_modified();
			mark_as_resized();
		}

		void						reduce_if_possible(				const uint32_t			AMOUNT)
		{
			restore();
			container.reduce_if_possible(AMOUNT);
			mark_as_modified();
			mark_as_resized();
		}

		void						resize(							const uint32_t			NEW_SIZE)
		{
			restore();
			container.resize(NEW_SIZE);
			mark_as_modified();
			mark_as_resized();
		}

		void						swap_elements(					const uint32_t			FIRST_INDEX,
																	const uint32_t			SECOND_INDEX)
		{
			if(FIRST_INDEX == SECOND_INDEX) return;
			restore();
			container.swap_elements(FIRST_INDEX, SECOND_INDEX);
			mark_as_modified();
		}

		inline void					make_last(						const uint32_t			INDEX)
		{
			restore();
			container.make_last(INDEX);
			mark_as_modified();
		}

		void						fast_erase(						const uint32_t			ELEMENT_INDEX)
		{
			restore();
			container.fast_erase(ELEMENT_INDEX);
			mark_as_modified();
			mark_as_resized();
		}

		T*							rearrange(						const dpl::DeltaArray&	DELTA)
		{
			restore();
			container.rearrange(DELTA);
			mark_as_modified();
			return container.data();
		}

		inline void					destroy_all_elements()
		{
			container.clear();
			mark_as_modified();
			mark_as_resized();
		}

		void						import_from(					std::istream&			binary)
		{
			container.import_from(binary);
			mark_as_modified();
			mark_as_resized();
		}

		void						import_tail_from(				std::istream&			binary)
		{
			container.import_tail_from(binary);
			mark_as_modified();
			mark_as_resized();
		}

		void						export_to(						std::ostream&			binary) const
		{
			restore();
			container.export_to(binary);
		}

		void						export_tail_to(					std::ostream&			binary,
																	const uint32_t			TAIL_SIZE) const
		{
			restore();
			container.export_tail_to(binary, TAIL_SIZE);
		}

	private: // functions
		inline uint32_t				update_range(					const uint32_t			OFFSET) const
		{
			const uint32_t SIZE = range().size();
			range->reset(OFFSET, OFFSET + SIZE);
			return SIZE;
		}

		void						flush() const
		{
			if(!flags.at(NEEDS_FLUSH)) return;

			const MyStream&	TRANSFER = get_stream();
								TRANSFER.on_flush_array(range, container.data());
			
			flags.set_at(RESIZED,		false);
			flags.set_at(NEEDS_FLUSH,	false);
			if(flags.at(KEPT)) return;
			container.clear();
		}

		bool						restore(						const bool				bMODIFIED_ON_RESIZE = false) const
		{
			if(!is_stream_ready()) return false;
			if(!flags.at(KEPT) && !flags.at(NEEDS_FLUSH))
			{
				container.reserve(size());
				flags.set_at(RESIZED, false); //<-- Size was restored, any previous resize operation is lost.

				const MyStream&	STREAM = get_stream();
								STREAM.on_restore_array(range, container.data());
								STREAM.notify_modified();

				flags.set_at(NEEDS_FLUSH, true);
			}
			else if(bMODIFIED_ON_RESIZE)
			{
				flags.set_at(NEEDS_FLUSH, true); //<-- Flush forced by the transfer.
			}

			return true;
		}

		inline void					request_flush()
		{
			if(!is_stream_ready()) return;
			flags.set_at(NEEDS_FLUSH, true);
			get_stream().notify_modified();
		}

		inline void					request_new_offset()
		{
			if(!is_stream_ready()) return;
			get_stream().notify_resized();
		}

		inline void					mark_as_modified()
		{
			if(!flags.at(NEEDS_FLUSH)) request_flush();
		}

		inline void					mark_as_resized()
		{		
			range->reset(0, container.size());
			if(flags.at(RESIZED)) return;// Already marked as resized?
			flags.set_at(RESIZED, true);
			request_new_offset();
		}

	private: // debug exceptions
		inline void					throw_if_invalid_delta(			const dpl::DeltaArray&	DELTA) const
		{
#ifdef _DEBUG
			if(DELTA.size() != size())
				throw dpl::GeneralException(this, __LINE__, "Invalid delta.");
#endif // _DEBUG
		}
	};


	/*
		Handles transfer of data packs.
		Thread-safe as long as data is kept after flush.
	*/
	template<is_Streamable T>
	class	StreamController : private dpl::Group<StreamController<T>, StreamChunk<T>>
	{
	private: // subtypes
		using	MyType		= StreamController<T>;
		using	MyChunk		= StreamChunk<T>;
		using	MyChainBase	= dpl::Group<MyType, MyChunk>;
		using	MyLink		= dpl::Member<MyType, MyChunk>;

	public: // relations
		friend	MyChunk;
		friend	MyChainBase;
		friend	MyLink;

	public: // subtypes
		using	Callback		= std::function<void(MyChunk&)>;
		using	ConstCallback	= std::function<void(const MyChunk&)>;

	public: // data
		mutable dpl::ReadOnly<uint32_t, StreamController>	size; // Total number of data units.
		dpl::ReadOnly<bool, StreamController>				bKeepFlushedData;

	private: // data
		mutable std::atomic_bool							bResized;
		mutable std::atomic_bool							bNeedsFlush;

	protected: // lifecycle
		CLASS_CTOR				StreamController(		const bool						bKEEP_FLUSHED_DATA = true)
			: size(0)
			, bKeepFlushedData(bKEEP_FLUSHED_DATA)
			, bResized(false)
			, bNeedsFlush(false)
		{
			
		}
			
		CLASS_CTOR				StreamController(		const StreamController&			OTHER) = delete;
				
		CLASS_CTOR				StreamController(		StreamController&&				other) noexcept
			: MyChainBase(std::move(other))
			, size(other.size)
			, bKeepFlushedData(other.bKeepFlushedData)
			, bResized(other.bResized.load())
			, bNeedsFlush(other.bNeedsFlush.load())
		{

		}

		StreamController&		operator=(				const StreamController&			OTHER) = delete;

		StreamController&		operator=(				StreamController&&				other) noexcept
		{
			detach_all_chunks();
			MyChainBase::operator=(std::move(other));
			size				= other.size;
			bKeepFlushedData	= other.bKeepFlushedData;
			bResized.store(other.bResized.load());
			bNeedsFlush.store(other.bNeedsFlush.load());
			return *this;
		}

	public: // functions
		bool					needs_update() const
		{
			return bResized.load(std::memory_order_relaxed) || bNeedsFlush.load(std::memory_order_relaxed);
		}

		bool					add_chunk(				MyChunk&						chunk)
		{
			if(!MyChainBase::attach_back(chunk)) return false;
			chunk.flags.set_at(TransferFlags::KEPT, bKeepFlushedData);
			notify_resized();
			return true;
		}

		void					for_each_chunk(			const Callback&					FUNCTION)
		{
			MyChainBase::iterate_forward(FUNCTION);
		}

		void					for_each_chunk(			const ConstCallback&			FUNCTION) const
		{
			MyChainBase::iterate_forward(FUNCTION);
		}

		void					detach_chunk(			MyChunk&						chunk)
		{
			if(chunk.is_linked_to(*this)) chunk.detach_from_stream();
		}

		void					detach_all_chunks()
		{
			while(MyChunk* current = MyChainBase::first())
			{
				detach_chunk(*current);
			}
		}

	protected: // functions
		void					update() const
		{
			update_size();

			if(bNeedsFlush.load(std::memory_order_relaxed)) //<-- At least one array needs flush.
			{
				StreamController::for_each_chunk([&](const MyChunk& PACK)
				{
					PACK.flush();
				});
				bNeedsFlush.store(false, std::memory_order_relaxed);
				on_updated();
			}
		}

	private: // interface
		virtual void			on_transfer_requested() const{}

		virtual void			on_resized() const{}

		virtual void			on_flush_array(			const dpl::IndexRange<uint32_t>	RANGE,
														const T*						SOURCE) const{}

		virtual void			on_restore_array(		const dpl::IndexRange<uint32_t>	RANGE,
														T*								target) const{}

		virtual void			on_updated() const{}

	private: // functions
		void					notify_resized() const
		{
			bResized.store(true, std::memory_order_relaxed);
		}

		void					notify_modified() const
		{
			bNeedsFlush.store(true, std::memory_order_relaxed);
			on_transfer_requested();
		}

		void					update_size() const
		{
			if(bResized.load(std::memory_order_relaxed))
			{
				size = 0;
				StreamController::for_each_chunk([&](const MyChunk& PACK)
				{
					PACK.restore(true); //<-- Make sure that data is restored before next flush.
					*size += PACK.update_range(size);
				});
				on_resized(); //<-- Notify derived class
				bResized.store(false, std::memory_order_relaxed);
				bNeedsFlush.store(true, std::memory_order_relaxed);			
			}
		}
	};
}

#pragma pack(pop)