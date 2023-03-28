#pragma once


#include <array>
#include <vector>
#include <functional>
#include "dpl_ReadOnly.h"
#include "dpl_GeneralException.h"
#include "dpl_Swap.h"
#include "dpl_DynamicBuffer.h"
#include "dpl_TypeTraits.h"

#pragma pack(push, 4)

// declarations
namespace dpl
{
	template<typename OwnerT, typename PropertyT>
	class	Property;

	class	Ownership;

	template<typename PropertyT>
	concept ValidProperty = dpl::has_assignment_operator<PropertyT, Swap<PropertyT>> && std::is_constructible_v<PropertyT, const Ownership&, PropertyT&&>;

	template<typename OwnerT, typename PropertyT>
	class	Owner;

	template<typename OwnerT, ValidProperty PropertyT, uint32_t MAX_PROPERTIES>
	class	StaticOwner;

	template<typename OwnerT, ValidProperty PropertyT>
	class	DynamicOwner;
}

// definitions
namespace dpl
{
	enum	PropertyOrder
	{
		IGNORED		= 0,// Owners will use fast removal to delete objects in the middle of the array(object in the middle is swapped with the last object before deletion).
		PRESERVED	= 1	// When object is removed from the middle of an array, all objects are shifted by one place.
	};

	/*
		Holds index of the property.
	*/
	class	Ownership
	{
	public: // friends
		template<typename, typename>
		friend class Owner;

		template<typename, typename>
		friend class Property;

		template<typename, ValidProperty>
		friend class DynamicOwner;

		template<typename, ValidProperty, uint32_t> 
		friend class StaticOwner;

	public: // constants
		static const uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

	public: // data
		ReadOnly<uint32_t, Ownership> index;

	protected: // lifecycle
		CLASS_CTOR	Ownership()
			: index(INVALID_INDEX)
		{

		}

		CLASS_CTOR	Ownership(const Ownership&	OTHER)
			: index(OTHER.index())
		{

		}

	private: // lifecycle
		CLASS_CTOR	Ownership(const uint32_t	INDEX)
			: index(INDEX)
		{

		}
	};


	/*
		Objects of this class are owned by one of the Owner class objects.
		Stores self updating pointer to the owner.
	*/
	template<typename OwnerT, typename PropertyT>
	class	Property
	{
	public: // subtypes
		using	MyOwner	= Owner<OwnerT, PropertyT>;

	public: // relations
		friend	MyOwner;

		template<typename, ValidProperty> 
		friend class DynamicOwner;

		template<typename, ValidProperty, uint32_t> 
		friend class StaticOwner;

	public: // constants
		static const uint32_t INVALID_INDEX = Ownership::INVALID_INDEX;

	private: // data
		uint32_t m_index;

	protected: // lifecycle
		CLASS_CTOR				Property(		const Ownership&	OWNERSHIP)
			: m_index(OWNERSHIP.index())
		{

		}

		CLASS_CTOR				Property(		const Ownership&	OWNERSHIP, 
												Property&&			other) noexcept
			: Property(OWNERSHIP)
		{
			other.invalidate();
		}

		CLASS_DTOR				~Property() = default;

		inline Property&		operator=(		Swap<Property>		other)
		{
			return *this; // Note that index is not swapped!
		}

		inline Property&		operator=(		Swap<PropertyT>		other)
		{
			return operator=(Swap<Property>(*other));
		}

	private: // lifecycle
		CLASS_CTOR				Property(		const Property&		OTHER) = delete;

		Property&				operator=(		const Property&		OTHER) = delete;

		Property&				operator=(		Property&&			other) noexcept = delete;

	public: // functions
		inline bool				has_owner() const
		{
			return m_index != INVALID_INDEX;
		}

		inline bool				is_owned_by(	const MyOwner*		OWNER) const
		{
			return get_raw_owner_ptr() == OWNER;
		}

		inline bool				is_owned_by(	const MyOwner&		OWNER) const
		{
			return get_raw_owner_ptr() == &OWNER;
		}

		inline const OwnerT&	get_owner() const
		{
			return *get_raw_owner_ptr()->cast();
		}

		inline uint32_t			get_index() const
		{
			return m_index;
		}

	protected: // functions
		inline OwnerT&			get_owner()
		{
			return *get_raw_owner_ptr()->cast();
		}

	private: // functions
		inline MyOwner*			get_raw_owner_ptr()
		{
			return *reinterpret_cast<MyOwner**>(reinterpret_cast<uint8_t*>(cast() - m_index) - sizeof(uint8_t*));
		}

		inline const MyOwner*	get_raw_owner_ptr() const
		{
			return *reinterpret_cast<const MyOwner*const*>(reinterpret_cast<const uint8_t*>(cast() - m_index) - sizeof(const uint8_t*));
		}

		inline PropertyT*		cast()
		{
			return static_cast<PropertyT*>(this);
		}

		inline const PropertyT*	cast() const
		{
			return static_cast<const PropertyT*>(this);
		}

		inline void				invalidate()
		{
			m_index = INVALID_INDEX;
		}
	};


	template<typename OwnerT, typename PropertyT>
	class	Owner
	{
	private: // subtypes
		using MyProperty	= Property<OwnerT, PropertyT>;

	public: // relations
		friend Property<OwnerT, PropertyT>;

	private: // functions
		inline OwnerT*			cast()
		{
			return static_cast<OwnerT*>(this);
		}

		inline const OwnerT*	cast() const
		{
			return static_cast<const OwnerT*>(this);
		}
	};


	/*
		Owns collection of properties.
		Properties are stored in a continuous block of memory that cannot be modified at runtime.
	*/
	template<typename OwnerT, ValidProperty PropertyT, uint32_t _MAX_PROPERTIES>
	class	StaticOwner : public Owner<OwnerT, PropertyT>
	{
	public: // constants
		static const uint32_t MAX_PROPERTIES = _MAX_PROPERTIES;

	private: // subtypes
		using MyBase				= Owner<OwnerT, PropertyT>;
		using MyProperty			= Property<OwnerT, PropertyT>;
		using MyBuffer				= std::array<uint8_t, sizeof(MyBase*) + MAX_PROPERTIES * sizeof(PropertyT)>;

	public: // subtypes
		using InvokeProperty		= std::function<void(PropertyT&)>;
		using InvokeConstProperty	= std::function<void(const PropertyT&)>;
		using Condition				= std::function<bool(PropertyT&)>;
		using ConstCondition		= std::function<bool(const PropertyT&)>;

	public: // data
		ReadOnly<uint32_t, StaticOwner> numProperties;

	private: // data
		MyBuffer m_buffer;

	protected: // lifecycle
		CLASS_CTOR					StaticOwner()
			: numProperties(0)
		{
			*reinterpret_cast<MyBase**>(m_buffer.data()) = this;
		}

		CLASS_CTOR					StaticOwner(				const StaticOwner&				OTHER) = delete;

		CLASS_CTOR					StaticOwner(				StaticOwner&&					other) noexcept
			: StaticOwner()
		{
			move_properties(std::move(other));
		}

		CLASS_DTOR					~StaticOwner()
		{
			destroy_all_properties();
		}

		StaticOwner&				operator=(					const StaticOwner&				OTHER) = delete;

		StaticOwner&				operator=(					StaticOwner&&					other) noexcept
		{
			if(this != &other)
			{
				destroy_all_properties();	
				move_properties(std::move(other));
			}

			return *this;
		}

		StaticOwner&				operator=(					Swap<StaticOwner>				other)
		{
			StaticOwner* dst = this;		// Owner with smaller number of properties.
			StaticOwner* src = other.get();	// Owner with larger number of properties.
			
			if(dst->numProperties() > src->numProperties())
			{
				std::swap(dst, src);
			}

			// Get raw array of properties(avoids repetitive virtual calls to the buffer).
			PropertyT*const	dstProperties	= dst->get_properties();
			PropertyT*const	srcProperties	= src->get_properties();

			const uint32_t SWAP_RANGE = dst->numProperties();
			for(uint32_t index = 0; index < SWAP_RANGE; ++index)
			{
				dstProperties[index] = Swap(srcProperties[index]);
			}

			const uint32_t MOVE_RANGE = src->numProperties();
			for(uint32_t index = SWAP_RANGE; index < MOVE_RANGE; ++index)
			{
				auto& property = srcProperties[index];
				const Ownership NEW_OWNERSHIP(index);
				new(dstProperties+index)PropertyT(NEW_OWNERSHIP, std::move(property));
				property.~PropertyT();
			}

			numProperties.swap(other->numProperties);
			return *this;
		}

		inline StaticOwner&			operator=(					Swap<OwnerT>					other)
		{
			return operator=(Swap<StaticOwner>(*other));
		}

	public: // functions
		inline bool					is_empty() const
		{
			return numProperties() == 0;
		}

		inline bool					is_full() const
		{
			return numProperties() == MAX_PROPERTIES;
		}

	public: // access
		inline const PropertyT*		get_properties() const
		{
			return reinterpret_cast<const PropertyT*>(m_buffer.data() + sizeof(StaticOwner*));
		}

		inline const PropertyT&		get_property_at(			const uint32_t					PROPERTY_INDEX) const
		{
			return get_properties()[PROPERTY_INDEX];
		}

		inline const PropertyT&		get_first_property() const
		{
			return get_property_at(0);
		}

		inline const PropertyT&		get_last_property() const
		{
			return get_property_at(numProperties() - 1);
		}

	protected: // access
		inline PropertyT*			get_properties()
		{
			return reinterpret_cast<PropertyT*>(m_buffer.data() + sizeof(StaticOwner*));
		}

		inline PropertyT&			get_property_at(			const uint32_t					PROPERTY_INDEX)
		{
			return get_properties()[PROPERTY_INDEX];
		}

		inline PropertyT&			get_first_property()
		{
			return get_property_at(0);
		}

		inline PropertyT&			get_last_property()
		{
			return get_property_at(numProperties() - 1);
		}

	public: // iteration
		inline const PropertyT*		for_each_property_until(	const ConstCondition&			CONDITION) const
		{
			const auto* END		= get_properties() + numProperties();
			const auto* CURRENT	= get_properties();
			while(CURRENT != END) if(CONDITION(*CURRENT)) return CURRENT;
			return nullptr;
		}

		inline void					for_each_property(			const InvokeConstProperty&		FUNCTION) const
		{
			const auto* END		= get_properties() + numProperties();
			const auto* CURRENT	= get_properties();
			while(CURRENT != END) FUNCTION(*CURRENT);
			return nullptr;
		}

	protected: // iteration
		inline PropertyT*			for_each_property_until(	const Condition&				condition)
		{
			const auto* END	= get_properties() + numProperties();
			auto* current	= get_properties();
			while(current != END) if(condition(*current)) return current;
			return nullptr;
		}

		inline void					for_each_property(			const InvokeProperty&			function)
		{
			const auto* END	= get_properties() + numProperties();
			auto* current	= get_properties();
			while(current != END) function(*current);
			return nullptr;
		}

	protected: // property managment
		template<typename... CTOR>
		inline PropertyT&			create_property(			CTOR&&...						args)
		{
			if(numProperties() == MAX_PROPERTIES) throw GeneralException(this, __LINE__, "Too many properties.");
			return create_property_internal(std::forward<CTOR>(args)...);
		}

		inline bool					swap_properties(			const uint32_t					FIRST_INDEX,
																const uint32_t					SECOND_INDEX)
		{
			if(FIRST_INDEX == SECOND_INDEX) return false;
			get_property_at(FIRST_INDEX) = Swap(get_property_at(SECOND_INDEX));
			return true;
		}

		/*
			Performs insertion sort on the properties based on the comparsion specified by the user.
			Comparator accepts properties in their current order and swaps them on true.
		*/
		template<typename CompareT>
		void						sort_properties(			CompareT						comp)
		{
			PropertyT* first = get_properties();
			for(uint64_t currentID = 1; currentID < numProperties(); ++currentID)
			{
				PropertyT* current	= first + currentID;
				PropertyT* previous = current - 1;

				while(current != first)
				{
					if(comp(*previous, *current))
						break;

					*current = Swap(*previous);
					--previous;
					current = previous+1;
				}
			}
		}

		bool						destroy_property_at(		const uint32_t					TARGET_INDEX,
																const PropertyOrder				PROPERTY_ORDER = PropertyOrder::IGNORED)
		{
			
			if(TARGET_INDEX >= numProperties()) return false;

			const uint32_t LAST_INDEX = numProperties() - 1;

			if(TARGET_INDEX != LAST_INDEX)
			{
				if(PROPERTY_ORDER == PropertyOrder::PRESERVED)
				{
					// We have to shift properties towards the beginning.
					uint32_t currentIndex = TARGET_INDEX;
					while(currentIndex != LAST_INDEX)
					{
						const uint32_t NEXT_INDEX = currentIndex + 1;
						get_property_at(currentIndex) = Swap(get_property_at(NEXT_INDEX));
						currentIndex = NEXT_INDEX;
					}
				}
				else // PropertyOrder::IGNORED
				{
					get_property_at(LAST_INDEX) = Swap(get_property_at(TARGET_INDEX));
				}
			}

			get_property_at(--(*numProperties)).~PropertyT();
			return true;
		}

		inline bool					destroy_property(			MyProperty&						object,
																const PropertyOrder				PROPERTY_ORDER = PropertyOrder::IGNORED)
		{
			return (object.m_owner == this) ? destroy_property_at(object.get_index(), PROPERTY_ORDER) : false;
		}

		/*
		*	Destroys properties for whom the condition is met.
		*/
		inline void					destroy_property_if(		const PropertyOrder				PROPERTY_ORDER,
																const ConstCondition&			CONDITION)
		{
			uint64_t currentID = 0;
			while(currentID != numProperties())
			{
				CONDITION(get_properties()[currentID]) ? destroy_property_at(currentID, PROPERTY_ORDER) : ++currentID;
			}
		}

		inline bool					destroy_all_properties()
		{
			if(numProperties() == 0) return false;
			while(numProperties() > 0)
			{
				destroy_property_at(numProperties()-1);
			}
			return true;
		}

	private: // functions
		template<typename... CTOR>
		inline PropertyT&			create_property_internal(	CTOR&&...						args)
		{
			const Ownership NEW_OWNERSHIP((*numProperties)++);
			auto& obj = get_properties()[NEW_OWNERSHIP.index()];
			new(&obj)PropertyT(NEW_OWNERSHIP, std::forward<CTOR>(args)...);
			return obj;
		}

		template<typename OwnershipWrapperT, typename... CTOR>
		inline PropertyT&			create_property_internal(	OwnershipWrapperT&&				wrapper,
																CTOR&&...						args)
		{
			const Ownership NEW_OWNERSHIP((*numProperties)++);
			auto& obj = get_properties()[NEW_OWNERSHIP.index()];
			if constexpr( !std::is_const_v<OwnershipWrapperT> && std::is_base_of_v<Ownership, OwnershipWrapperT>)
			{
				static_cast<Ownership&>(wrapper) = NEW_OWNERSHIP;
				new(&obj)PropertyT(std::forward<OwnershipWrapperT>(wrapper), std::forward<CTOR>(args)...);
			}
			else
			{
				new(&obj)PropertyT(NEW_OWNERSHIP, std::forward<CTOR>(args)...);
			}

			return obj;
		}

		inline void					move_properties(			StaticOwner&&					other)
		{
			numProperties = other.numProperties();
			for(uint32_t index = 0; index < numProperties(); ++index) // NOTE: Variable 'numProperties' was already moved.
			{
				auto& property = other.get_property_at(index);
				const Ownership NEW_OWNERSHIP(index);
				new(get_properties()+index)PropertyT(NEW_OWNERSHIP, std::move(property));
				property.~PropertyT();
			}
			other.numProperties = 0;
		}
	};


	/*
		Owns collection of properties.
		Properties are stored in a continuous block of memory that can be modified at runtime.
	*/
	template<typename OwnerT, ValidProperty PropertyT>
	class	DynamicOwner : public Owner<OwnerT, PropertyT>
	{
	private: // subtypes
		using MyBase				= Owner<OwnerT, PropertyT>;
		using MyProperty			= Property<OwnerT, PropertyT>;
		using MyBuffer				= DynamicBuffer<PropertyT, sizeof(MyBase*)>;

	public: // subtypes
		using InvokeProperty		= std::function<void(PropertyT&)>;
		using InvokeConstProperty	= std::function<void(const PropertyT&)>;
		using Condition				= std::function<bool(PropertyT&)>;
		using ConstCondition		= std::function<bool(const PropertyT&)>;

	public: // data
		ReadOnly<uint32_t,	DynamicOwner> numProperties;

	private: // data
		MyBuffer m_buffer;

	protected: // lifecycle
		CLASS_CTOR					DynamicOwner()
			: numProperties(0)
			, m_buffer(2)
		{
			m_buffer.header<MyBase*>() = this;
		}

		CLASS_CTOR					DynamicOwner(				const DynamicOwner&			OTHER) = delete;

		CLASS_CTOR					DynamicOwner(				DynamicOwner&&				other) noexcept
			: numProperties(other.numProperties)
			, m_buffer(std::move(other.m_buffer))
		{
			notify_moved();
		}

		CLASS_DTOR					~DynamicOwner() = default;

		DynamicOwner&				operator=(					const DynamicOwner&			OTHER) = delete;

		DynamicOwner&				operator=(					DynamicOwner&&				other) noexcept
		{
			destroy_all_properties();
			numProperties	= other.numProperties;
			m_buffer		= std::move(other.m_buffer);
			m_buffer.header<MyBase*>() = this;
			notify_moved();
			return *this;
		}

		inline DynamicOwner&		operator=(					Swap<DynamicOwner>			other)
		{
			swap_properties(*other);
			return *this;
		}

		inline DynamicOwner&		operator=(					Swap<OwnerT>				other)
		{
			return operator=(Swap<DynamicOwner>(*other));
		}

	public: // functions
		inline bool					is_empty() const
		{
			return numProperties() == 0;
		}

		inline bool					is_full() const
		{
			return numProperties() == m_buffer.capacity();
		}

		inline uint32_t				capacity() const
		{
			return m_buffer.capacity();
		}

		inline bool					reserve_properties(			const uint32_t				NUM_PROPERTIES)
		{
			if(NUM_PROPERTIES <= m_buffer.capacity()) return false;
			relocate_properties(NUM_PROPERTIES);
			return true;
		}

	protected: // property management
		inline void					swap_properties(			DynamicOwner&				other)
		{
			numProperties.swap(other.numProperties);
			m_buffer.swap(other.m_buffer);
			this->notify_moved();
			other.notify_moved();
		}

		/*
			Creates property from the given set of arguments.
		*/
		template<typename... CTOR>
		inline PropertyT&			create_property(			CTOR&&...					args)
		{
			if(this->is_full()) reserve_properties(2 * numProperties());
			return create_property_internal(std::forward<CTOR>(args)...);
		}

		inline bool					swap_properties_at(			const uint32_t				FIRST_INDEX,
																const uint32_t				SECOND_INDEX)
		{
			if(FIRST_INDEX == SECOND_INDEX) return false;
			get_property_at(FIRST_INDEX) = Swap(get_property_at(SECOND_INDEX));
			return true;
		}

		/*
			Performs insertion sort on the properties based on the comparsion specified by the user.
			Comparator accepts properties in their current order and swaps them on true.
		*/
		template<typename CompareT>
		void						sort_properties(			CompareT&&					comp)
		{
			PropertyT* first = get_properties();
			for(uint64_t currentID = 1; currentID < numProperties(); ++currentID)
			{
				PropertyT* current	= first + currentID;
				PropertyT* previous = current - 1;

				while(current != first)
				{
					if(comp(*previous, *current))
						break;

					*current = Swap(*previous);
					--previous;
					current = previous+1;
				}
			}
		}

		inline bool					destroy_last_property()
		{
			if(numProperties() == 0) return false;
			m_buffer.destroy_at(--(*numProperties));
			return true;
		}

		bool						destroy_property_at(		const uint32_t				TARGET_INDEX,
																const PropertyOrder			PROPERTY_ORDER = PropertyOrder::IGNORED)
		{		
			if(TARGET_INDEX >= numProperties()) return false;

			const uint32_t LAST_INDEX = numProperties() - 1;

			if(TARGET_INDEX != LAST_INDEX)
			{
				if(PROPERTY_ORDER == PropertyOrder::PRESERVED)
				{
					// We have to shift properties towards the beginning.
					uint32_t currentIndex = TARGET_INDEX;
					while(currentIndex != LAST_INDEX)
					{
						const uint32_t NEXT_INDEX = currentIndex + 1;
						get_property_at(currentIndex) = Swap(get_property_at(NEXT_INDEX));
						currentIndex = NEXT_INDEX;
					}
				}
				else // PropertyOrder::IGNORED
				{
					get_property_at(LAST_INDEX) = Swap(get_property_at(TARGET_INDEX));
				}
			}

			return destroy_last_property();
		}

		inline bool					destroy_property(			MyProperty&					object,
																const PropertyOrder			PROPERTY_ORDER = PropertyOrder::IGNORED)
		{
			return object.is_owned_by(this) ? destroy_property_at(object.get_index(), PROPERTY_ORDER) : false;
		}

		/*
		*	Destroys properties for whom the condition is met.
		*/
		inline void					destroy_property_if(		const ConstCondition&		CONDITION,
																const PropertyOrder			PROPERTY_ORDER = PropertyOrder::IGNORED)
		{
			uint32_t currentID = 0;
			while(currentID != numProperties())
			{
				CONDITION(get_property_at(currentID)) ? destroy_property_at(currentID, PROPERTY_ORDER) : ++currentID;
			}
		}

		bool						destroy_all_properties()
		{
			if(numProperties() > 0)
			{
				for(uint32_t index = 0; index < numProperties(); ++index)
				{
					m_buffer.destroy_at(--(*numProperties));
				}
				numProperties = 0;
				return true;
			}

			return false;
		}

		inline void					shrink_to_fit_properties()
		{
			if(numProperties() < m_buffer.capacity())
			{
				relocate_properties(numProperties());
			}
		}

	public: // access
		inline const PropertyT*		get_properties() const
		{
			return m_buffer.data();
		}

		inline const PropertyT&		get_property_at(			const uint32_t				PROPERTY_INDEX) const
		{
			return get_properties()[PROPERTY_INDEX];
		}

		inline const PropertyT&		get_first_property() const
		{
			return get_property_at(0);
		}

		inline const PropertyT&		get_last_property() const
		{
			return get_property_at(numProperties() - 1);
		}

	protected: // access
		inline PropertyT*			get_properties()
		{
			return m_buffer.data();
		}

		inline PropertyT&			get_property_at(			const uint32_t				PROPERTY_INDEX)
		{
			return get_properties()[PROPERTY_INDEX];
		}

		inline PropertyT&			get_first_property()
		{
			return get_property_at(0);
		}

		inline PropertyT&			get_last_property()
		{
			return get_property_at(numProperties() - 1);
		}

	public: // iteration
		inline const PropertyT*		for_each_property_until(	const ConstCondition&		CONDITION) const
		{
			const auto* END		= get_properties() + numProperties();
			const auto*	CURRENT = get_properties();
			while(CURRENT != END)
			{
				if(CONDITION(*CURRENT)) return CURRENT;
				CURRENT++;
			}
			return nullptr;
		}

		inline void					invoke_properties_forward(	const InvokeConstProperty&	INVOKE) const
		{
			const auto* END		= get_properties() + numProperties();
			const auto*	CURRENT = get_properties();
			while(CURRENT != END) INVOKE(*(CURRENT++));
		}

		inline void					invoke_properties_backward(	const InvokeConstProperty&	INVOKE) const
		{
			if(this->is_empty()) return;
			const auto* IT		= get_properties() + numProperties();
			const auto*	LAST	= get_properties();
			while(IT >= LAST) INVOKE(*(--IT));
		}

		inline void					for_each_property(			const InvokeConstProperty&	INVOKE) const
		{
			DynamicOwner::invoke_properties_forward(INVOKE);
		}

	protected: // iteration
		inline PropertyT*			for_each_property_until(	const Condition&			condition)
		{
			const auto* END		= get_properties() + numProperties();
			auto*		current = get_properties();
			while(current != END)
			{
				if(condition(*current)) return current;
				current++;
			}
			return nullptr;
		}

		inline void					invoke_properties_forward(	const InvokeProperty&		INVOKE) const
		{
			const auto* END		= get_properties() + numProperties();
			auto*		current = get_properties();
			while(current != END) INVOKE(*(current++));
		}

		inline void					invoke_properties_backward(	const InvokeProperty&		INVOKE)
		{
			if(this->is_empty()) return;
			auto*		it		= get_properties() + numProperties();
			const auto*	LAST	= get_properties();
			while(it >= LAST) INVOKE(*(--it));
		}

		inline void					for_each_property(			const InvokeProperty&		INVOKE)
		{
			DynamicOwner::invoke_properties_forward(INVOKE);
		}

	private: // functions
		template<typename... CTOR>
		inline PropertyT&			create_property_internal(	CTOR&&...					args)
		{
			const Ownership NEW_OWNERSHIP((*numProperties)++);
			return m_buffer.construct_at(NEW_OWNERSHIP.index(), NEW_OWNERSHIP, std::forward<CTOR>(args)...);
		}

		template<typename OwnershipWrapperT, typename... CTOR>
		inline PropertyT&			create_property_internal(	OwnershipWrapperT&&			wrapper,
																CTOR&&...					args)
		{
			const Ownership NEW_OWNERSHIP((*numProperties)++);
			if constexpr( !std::is_const_v<OwnershipWrapperT> && std::is_base_of_v<Ownership, OwnershipWrapperT>)
			{
				static_cast<Ownership&>(wrapper) = NEW_OWNERSHIP;
				return m_buffer.construct_at(NEW_OWNERSHIP.index(), std::forward<OwnershipWrapperT>(wrapper), std::forward<CTOR>(args)...);
			}
			else
			{
				return m_buffer.construct_at(NEW_OWNERSHIP.index(), NEW_OWNERSHIP, std::forward<CTOR>(args)...);
			}
		}

		void						relocate_properties(		const uint32_t				NEW_CAPACITY)
		{
			if(NEW_CAPACITY >= numProperties())
			{
				MyBuffer	newBuffer(NEW_CAPACITY);
							newBuffer.header<MyBase*>() = this;

				for(uint32_t index = 0; index < numProperties(); ++index)
				{
					auto& property = m_buffer[index];
					const Ownership NEW_OWNERSHIP(index);
					new(newBuffer.data()+index)PropertyT(NEW_OWNERSHIP, std::move(property));
					property.~PropertyT();
				}
			
				m_buffer.swap(newBuffer);
			}
		}

		inline void					notify_moved()
		{
			m_buffer.header<MyBase*>() = this;
		}
	};
}

#pragma pack(pop)