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
	class	Stream;
}

// definitions
namespace dpl
{
	template<is_Streamable T>
	class	StreamChunk	: private dpl::Member<Stream<T>, StreamChunk<T>>
	{
	private:	// [SUBTYPES]
		using	MyType			= StreamChunk<T>;
		using	MyStream		= Stream<T>;
		using	MyMemberBase	= dpl::Member<MyStream, MyType>;
		using	MyGroup			= dpl::Group<MyStream, MyType>;

		enum	Flags
		{
			RESIZED, // chunk was resized and stream needs to be reorganized
			ALWAYS_SYNCHRONIZED, // data is kept at both sides
			MODIFIED_LOCALLY, // one or more item was modified locally
			MODIFIED_REMOTELY, // one or more item was modified remotely
		};

	public:		// [FRIENDS]
		friend	MyStream;
		friend	MyMemberBase;
		friend	MyGroup;
		friend	dpl::Sequenceable<MyType>;

	public:		// [SUBTYPES]
		using	OnModify	= std::function<void(dpl::Buffer<T>&)>;
		using	Range		= dpl::IndexRange<uint32_t>;
		using	Invoke		= typename dpl::DynamicArray<T>::Invoke;
		using	InvokeConst	= typename dpl::DynamicArray<T>::InvokeConst;

	public:		// [DATA]
		mutable dpl::ReadOnly<Range, StreamChunk>	range; // Note: May be invalid if transfer was not yet updated.

	private:	// [DATA]
		mutable dpl::DynamicArray<T>				container;
		mutable dpl::Mask32_t						flags;	

	public:		// [LIFECYCLE]
		CLASS_CTOR					StreamChunk()
		{
			flags.set_at(ALWAYS_SYNCHRONIZED, true);
		}

		CLASS_CTOR					StreamChunk(				const StreamChunk&		OTHER)
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

		CLASS_CTOR					StreamChunk(				StreamChunk&&			other) noexcept
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

		StreamChunk&				operator=(					const StreamChunk&		OTHER)
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

		StreamChunk&				operator=(					StreamChunk&&			other) noexcept
		{
			MyMemberBase::operator=(std::move(other));
			range.swap(other.range);
			container.swap(other.container);
			std::swap(flags, other.flags);
			return *this;
		}

	public:		// [TRANSFER INFO]
		bool						is_stream_ready() const
		{
			return MyMemberBase::is_member();
		}

		MyStream&					get_stream()
		{
			return *MyMemberBase::get_group();
		}

		const MyStream&				get_stream() const
		{
			return *MyMemberBase::get_group();
		}

		void						detach_from_stream()
		{
			if(restore())
			{
				notify_resized();
				MyMemberBase::detach();
				flags.set_at(ALWAYS_SYNCHRONIZED,	true);
				flags.set_at(RESIZED,				false);
				flags.set_at(MODIFIED_LOCALLY,		false);
				flags.set_at(MODIFIED_REMOTELY,		false);
			}
		}

	public:		// [GENERAL FUNCTIONS]
		uint32_t					offset() const
		{
			return range().begin();
		}

		uint32_t					size() const
		{
			return range().size();
		}

		uint32_t					capacity() const
		{
			return container.capacity();
		}

		/*
			Modify returned array.
			Warning! Returned pointer may be invalidated when transfer is updated or array is resized.
		*/
		T*							modify()
		{
			restore();
			notify_modified_locally();
			return container.data();
		}

		void						modify_each(				const Invoke&			INVOKE)
		{
			modify();
			container.for_each(INVOKE);
		}

		/*
			Get read-only data.
			Warning! Returned pointer may be invalidated when transfer is updated or array is resized.
		*/
		const T*					read() const
		{
			restore();
			return container.data();
		}

		void						read_each(					const InvokeConst&		INVOKE) const
		{
			read();
			container.for_each(INVOKE);
		}

		/*
			Returns INVALID_INDEX if out of range.
			WARNING! This function calls read.
		*/
		uint32_t					index_of(					const T*				ADDRESS) const
		{
			read();
			return container.index_of(ADDRESS);
		}

		template<typename... CTOR>
		T&							emplace_back(				CTOR&&...				args)
		{
			restore();
			T& newData = container.emplace_back(std::forward<CTOR>(args)...);
			notify_modified_locally();
			notify_resized();
			return newData;
		}

		T*							enlarge(					const uint32_t			AMOUNT)
		{
			restore();
			T* newData = container.enlarge(AMOUNT);
			notify_modified_locally();
			notify_resized();
			return newData;
		}

		void						reduce(						const uint32_t			AMOUNT)
		{
			restore();
			container.reduce(AMOUNT);
			notify_modified_locally();
			notify_resized();
		}

		void						reduce_if_possible(			const uint32_t			AMOUNT)
		{
			restore();
			container.reduce_if_possible(AMOUNT);
			notify_modified_locally();
			notify_resized();
		}

		void						resize(						const uint32_t			NEW_SIZE)
		{
			restore();
			container.resize(NEW_SIZE);
			notify_modified_locally();
			notify_resized();
		}

		void						swap_elements(				const uint32_t			FIRST_INDEX,
																const uint32_t			SECOND_INDEX)
		{
			if(FIRST_INDEX == SECOND_INDEX) return;
			restore();
			container.swap_elements(FIRST_INDEX, SECOND_INDEX);
			notify_modified_locally();
		}

		void						make_last(					const uint32_t			INDEX)
		{
			restore();
			container.make_last(INDEX);
			notify_modified_locally();
		}

		void						fast_erase(					const uint32_t			ELEMENT_INDEX)
		{
			restore();
			container.fast_erase(ELEMENT_INDEX);
			notify_modified_locally();
			notify_resized();
		}

		T*							rearrange(					const dpl::DeltaArray&	DELTA)
		{
			restore();
			container.rearrange(DELTA);
			notify_modified_locally();
			return container.data();
		}

		void						destroy_all_elements()
		{
			container.clear();
			notify_modified_locally();
			notify_resized();
		}

	public:		// [IMPORT/EXPORT]
		void						import_from(				std::istream&			binary)
		{
			container.import_from(binary);
			notify_modified_locally();
			notify_resized();
		}

		void						import_tail_from(			std::istream&			binary)
		{
			container.import_tail_from(binary);
			notify_modified_locally();
			notify_resized();
		}

		void						export_to(					std::ostream&			binary) const
		{
			restore();
			container.export_to(binary);
		}

		void						export_tail_to(				std::ostream&			binary,
																const uint32_t			TAIL_SIZE) const
		{
			restore();
			container.export_tail_to(binary, TAIL_SIZE);
		}

	private:	// [INTERNAL FUNCTIONS]
		bool						attach_to_stream(			MyStream&				stream)
		{
			detach_from_stream();
			if(!stream.attach_back(*this)) return false;
			flags.set_at(Flags::ALWAYS_SYNCHRONIZED, stream.bKeepFlushedData);
			stream.request_resize();
		}

		uint32_t					update_range(				const uint32_t			OFFSET) const
		{
			const uint32_t SIZE = range().size();
			range->reset(OFFSET, OFFSET + SIZE);
			return SIZE;
		}

		void						notify_modified_locally()
		{
			if(flags.at(MODIFIED_LOCALLY)) return;
			if(!is_stream_ready()) return;
			flags.set_at(MODIFIED_LOCALLY, true);
			get_stream().request_flush();
		}

		void						notify_resized()
		{		
			range->reset(0, container.size());
			if(flags.at(RESIZED)) return;// Already marked as resized?
			if(!is_stream_ready()) return;
			flags.set_at(RESIZED, true);
			get_stream().request_resize();
		}

		void						set_modified_remotely()
		{
			flags.set_at(MODIFIED_REMOTELY, true);
		}

		void						flush() const
		{
			if(!flags.at(MODIFIED_LOCALLY)) return;

			const MyStream&	STREAM = get_stream();
							STREAM.on_flush_array(range, container.data());
			
			flags.set_at(RESIZED,			false);
			flags.set_at(MODIFIED_LOCALLY,	false);
			flags.set_at(MODIFIED_REMOTELY, false); //<-- Information was lost!
			if(flags.at(ALWAYS_SYNCHRONIZED)) return;
			container.clear();
		}

		bool						restore(					const bool				bMODIFIED_ON_RESIZE = false) const
		{
			if(!is_stream_ready()) return false;
			if(flags.at(MODIFIED_REMOTELY) 
			|| !flags.at(ALWAYS_SYNCHRONIZED) && !flags.at(MODIFIED_LOCALLY))
			{
				container.reserve(size());

				const MyStream&	STREAM = get_stream();
								STREAM.on_restore_array(range, container.data());

				flags.set_at(RESIZED, false); //<-- Size was restored, any previous resize operation is lost.
				flags.set_at(MODIFIED_REMOTELY, false); //<-- Data is now synchronized.

				if(!flags.at(ALWAYS_SYNCHRONIZED)) //<-- User does not want to keep the data, force another flush.
				{
					flags.set_at(MODIFIED_LOCALLY, true);
					STREAM.request_flush();
				}
			}
			else if(bMODIFIED_ON_RESIZE)
			{
				flags.set_at(MODIFIED_LOCALLY, true); //<-- Flush forced by the transfer.
			}

			return true;
		}

	private:	// [DEBUG-ONLY]
		void						throw_if_invalid_delta(		const dpl::DeltaArray&	DELTA) const
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
	class	Stream : private dpl::Group<Stream<T>, StreamChunk<T>>
	{
	private:	// [SUBTYPES]
		using	MyType		= Stream<T>;
		using	MyChunk		= StreamChunk<T>;
		using	MyChainBase	= dpl::Group<MyType, MyChunk>;
		using	MyLink		= dpl::Member<MyType, MyChunk>;

	public:		// [FRIENDS]
		friend	MyChunk;
		friend	MyChainBase;
		friend	MyLink;

	public:		// [SUBTYPES]
		using	Invoke		= std::function<void(MyChunk&)>;
		using	InvokeConst	= std::function<void(const MyChunk&)>;
		using	IndexRange	= dpl::IndexRange<uint32_t>;

	public:		// [DATA]
		mutable dpl::ReadOnly<uint32_t, Stream>	size; // Total number of data units.
		dpl::ReadOnly<bool, Stream>				bKeepFlushedData;

	private:	// [DATA]
		mutable std::atomic_bool				bResized;
		mutable std::atomic_bool				bModifiedLocally;

	protected:	// [LIFECYCLE]
		CLASS_CTOR				Stream(						const bool			bKEEP_FLUSHED_DATA = true)
			: size(0)
			, bKeepFlushedData(bKEEP_FLUSHED_DATA)
			, bResized(false)
			, bModifiedLocally(false)
		{
			
		}
			
		CLASS_CTOR				Stream(						const Stream&		OTHER) = delete;
				
		CLASS_CTOR				Stream(						Stream&&			other) noexcept
			: MyChainBase(std::move(other))
			, size(other.size)
			, bKeepFlushedData(other.bKeepFlushedData)
			, bResized(other.bResized.load())
			, bModifiedLocally(other.bModifiedLocally.load())
		{

		}

		Stream&					operator=(					const Stream&		OTHER) = delete;

		Stream&					operator=(					Stream&&			other) noexcept
		{
			detach_all_chunks();
			MyChainBase::operator=(std::move(other));
			size				= other.size;
			bKeepFlushedData	= other.bKeepFlushedData;
			bResized.store(other.bResized.load());
			bModifiedLocally.store(other.bModifiedLocally.load());
			return *this;
		}

	public:		// [CHUNK HANDLING FUNCTIONS]
		bool					needs_update() const
		{
			return bResized.load(std::memory_order_relaxed) || bModifiedLocally.load(std::memory_order_relaxed);
		}

		bool					add_chunk(					MyChunk&			chunk)
		{
			return chunk.attach_to_stream(*this);
		}

		void					for_each_chunk(				const Invoke&		INVOKE)
		{
			MyChainBase::iterate_forward(INVOKE);
		}

		void					for_each_chunk(				const InvokeConst&	INVOKE) const
		{
			MyChainBase::iterate_forward(INVOKE);
		}

		void					detach_chunk(				MyChunk&			chunk)
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

	protected:	// [UPDATE]
		void					organize() const
		{
			if(bResized.load(std::memory_order_relaxed))
			{
				size = 0;
				Stream::for_each_chunk([&](const MyChunk& PACK)
				{
					PACK.restore(true); //<-- Make sure that data is restored before next flush.
					*size += PACK.update_range(size);
				});
				on_resized(); //<-- Notify derived class!
				bResized.store(false, std::memory_order_relaxed);
				bModifiedLocally.store(true, std::memory_order_relaxed);			
			}
		}

		void					update() const
		{
			organize();

			if(bModifiedLocally.load(std::memory_order_relaxed))
			{
				Stream::for_each_chunk([&](const MyChunk& PACK)
				{
					PACK.flush();
				});
				bModifiedLocally.store(false, std::memory_order_relaxed);
				on_updated();
			}
		}

		void					notify_modified_remotely()
		{
			// Note that we cannot store bModifiedRemotely state, since 
			// we do not know when will StreamChunk need to access its data.
			if(!bKeepFlushedData) return;
			Stream::for_each_chunk([&](MyChunk& pack)
			{
				pack.set_modified_remotely();
			});			
		}

	private:	// [INTERFACE]
		virtual void			on_transfer_requested() const{}

		virtual void			on_resized() const{}

		virtual void			on_flush_array(				const IndexRange	RANGE,
															const T*			SOURCE) const{}

		virtual void			on_restore_array(			const IndexRange	RANGE,
															T*					target) const{}

		virtual void			on_updated() const{}

	private:	// [INTERNAL FUNCTIONS]
		void					request_resize() const
		{
			bResized.store(true, std::memory_order_relaxed);
		}

		void					request_flush() const
		{
			bModifiedLocally.store(true, std::memory_order_relaxed);
			on_transfer_requested();
		}
	};
}

#pragma pack(pop)