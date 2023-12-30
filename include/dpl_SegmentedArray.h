#pragma once


#include <atomic>
#include <optional>
#include "dpl_ReadOnly.h"
#include "dpl_DynamicArray.h"
#include "dpl_Membership.h"
#include "dpl_Range.h"


// declarations
namespace dpl
{
	template<typename T>
	class SegmentedArray;

	template<typename T>
	class ArraySegment;
}

// definitions
namespace dpl
{
	template<typename T>
	class ArraySegment : private Member<SegmentedArray<T>, ArraySegment<T>>
	{
	private:	// [SUBTYPES]
		using	GroupType	= Group<SegmentedArray<T>, ArraySegment<T>>;
		using	MemberType	= Member<SegmentedArray<T>, ArraySegment<T>>;

	public:		// [FRIENDS]
		friend	MyGroup;
		friend	MyMember;
		friend	SegmentedArray<T>;
		friend	MemberType::MyBase;
		friend	MemberType::MySequence;
		friend	MemberType::MyLink;

	private:	// [DATA]
		std::optional<DynamicArray<T>>	buffer;
		IndexRange<uint32_t>			range;

	public:		// [LIFECYCLE]
		CLASS_CTOR					ArraySegment(		const uint32_t		INITIAL_SIZE)
			: buffer(std::in_place, INITIAL_SIZE)
		{
			
		}

		CLASS_CTOR					ArraySegment(		const ArraySegment& OTHER) = delete;

		CLASS_CTOR					ArraySegment(		ArraySegment&&		other) noexcept
			: MemberType(std::move(other))
			, buffer(std::move(other.buffer))
			, range(other.range)
		{
			other.range.reset();
		}

		CLASS_DTOR					~ArraySegment()
		{
			dpl::no_except([&]() 
			{
				remove_from_array();
			});
		}

		ArraySegment&				operator=(			const ArraySegment&	OTHER) = delete;

		ArraySegment&				operator=(			ArraySegment&&		other) noexcept
		{
			MemberType::operator=(std::move(other));
			buffer = std::move(other.buffer);
			range = other.range;
			other.range.reset();
			return *this;
		}

	public:		// [FUNCTIONS]
		bool						add_to_array(		SegmentedArray<T>&	array)
		{
			return array.add_segment(*this);
		}

		bool						remove_from_array()
		{
			if (auto* myArray = MemberType::get_group())
			{
				myArray->remove_segment(*this);
				return true;
			}
			return false;
		}

		uint32_t					size() const
		{
			return buffer.has_value()? buffer.value().size() : range.size();
		}

		T*							modify()
		{
			SegmentedArray<T>* myArray = MemberType::get_group();
			return myArray? myArray->modify() + range.begin() : buffer.value().data();
		}

		const T*					read() const
		{
			const SegmentedArray<T>* MY_ARRAY = MemberType::get_group();
			return MY_ARRAY? MY_ARRAY->read() + range.begin() : buffer.value().data();
		}
			
		void						resize(				const uint32_t		NEW_SIZE)
		{
			const uint32_t OLD_SIZE = size();
			if (NEW_SIZE != OLD_SIZE)
			{
				if (SegmentedArray<T>* myArray = MemberType::get_group())
				{
					myArray->on_segment_resized(*this, OLD_SIZE, NEW_SIZE);
				}
				buffer.value().resize(NEW_SIZE);
			}
		}

		SegmentedArray<T>*			get_array()
		{
			return MemberType::get_group();
		}

		const SegmentedArray<T>*	get_array() const
		{
			return MemberType::get_group();
		}
	};


	template<typename T>
	class SegmentedArray : private Group<SegmentedArray<T>, ArraySegment<T>>
	{
	private:	// [SUBTYPES]
		using	GroupType	= Group<SegmentedArray<T>, ArraySegment<T>>;
		using	MemberType	= Member<SegmentedArray<T>, ArraySegment<T>>;

	public:		// [FRIENDS]
		friend	MyGroup;
		friend	MyMember;
		friend	ArraySegment<T>;
		friend	GroupType::MyBase;

	public:		// [DATA]
		ReadOnly<std::atomic<uint32_t>, SegmentedArray<T>>	targetSize;
		ReadOnly<std::atomic_bool,		SegmentedArray<T>>	bModified;

	private:	// [DATA]
		ReadOnly<DynamicArray<T>,		SegmentedArray<T>>	buffer;

	public:		// [LIFECYCLE]
		CLASS_CTOR			SegmentedArray()
			: targetSize(0)
			, bModified(false)
		{

		}

		CLASS_CTOR			SegmentedArray(		const SegmentedArray&	OTHER) = delete;

		CLASS_CTOR			SegmentedArray(		SegmentedArray&&		other) noexcept
			: targetSize(other.targetSize->exchange(0))
			, bModified(other.bModified->exchange(false))
			, buffer(std::move(other.buffer))
		{

		}

		CLASS_DTOR			~SegmentedArray()
		{
			dpl::no_except([&]() 
			{
				remove_all_segments();
			});
		}

		SegmentedArray&		operator=(			const SegmentedArray&	OTHER) = delete;

		SegmentedArray&		operator=(			SegmentedArray&&		other) noexcept
		{
			targetSize = other.targetSize->exchange(0);
			bModified = other.bModified->exchange(false);
			buffer = std::move(other.buffer);
			return *this;
		}

	public:		// [FUNCTIONS]
		bool				add_segment(		ArraySegment<T>&		segment)
		{
			if (segment.get_array() != this)
			{
				segment.remove_from_array();
				GroupType::add_end_member(segment);
				targetSize += segment.size();
				bModified = true;
				return true;
			}
			return false;
		}

		bool				remove_segment(		ArraySegment<T>&		segment)
		{
			if (GroupType::remove_member(segment))
			{
				on_segment_resized(segment, segment.size(), 0);
				return true;
			}
			return false;
		}

		bool				remove_all_segments()
		{
			if (GroupType::empty()) return false;
			while (ArraySegment<T>* segment = GroupType::first())
			{
				remove_segment(*segment);
			}
			return true;
		}

		uint32_t			size() const
		{
			return buffer().size();
		}

		T*					modify()
		{
			bModified = true;
			return buffer->data();
		}

		const T*			read() const
		{
			return buffer().data();
		}

		void				update()
		{
			const uint32_t TARGET_SIZE = targetSize().load();
			if (TARGET_SIZE != buffer().size())
			{
				DynamicArray<T> newBuffer;
								newBuffer.reserve(TARGET_SIZE);

				GroupType::for_each([](ArraySegment<T>& segment)
				{
					const uint32_t OFFSET = newBuffer.size();
					const IndexRange<uint32_t> NEW_RANGE(OFFSET, OFFSET + segment.size());

					if (segment.buffer.has_value())
					{
						segment.buffer.value().for_each([&](const T& ELEMENT)
						{
							newBuffer.emplace_back(ELEMENT);
						});
						segment.buffer.reset();
					}
					else
					{
						segment.range.for_each([&](const uint32_t INDEX)
						{
							newBuffer.emplace_back(buffer()[INDEX]);
						});
					}

					segment.range = NEW_RANGE;
				});

				newBuffer.swap(*buffer);
			}
			bModified = false;
		}

	private:	// [FUNCTIONS]
		void				on_segment_resized(	ArraySegment<T>&		segment,
												const uint32_t			OLD_SIZE,
												const uint32_t			NEW_SIZE)
		{
			if (!segment.buffer.has_value())
			{
				segment.buffer.emplace(std::in_place, modify() + segment.range.begin(), segment.range.size());
				segment.range.reset();
			}
			targetSize -= OLD_SIZE;
			targetSize += NEW_SIZE;
		}
	};
}