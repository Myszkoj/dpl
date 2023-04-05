#pragma once


#include <stdint.h>
#include <functional>
#include <limits>
#include <type_traits>
#include <stdexcept>
#include <array>
#include "dpl_ReadOnly.h"
#include "dpl_Swap.h"


#pragma pack(push, 1)

// forward declarations
namespace dpl
{
	template<typename CompositeT, typename ComponentT, uint32_t N>
	class Component;

	template<typename CompositeT, typename ComponentT, uint32_t N>
	class Composite;
}

// implementations
namespace dpl
{
	/*
		NOTE: All derived classes have to override the Swap operator.
	*/
	template<typename CompositeT, typename ComponentT, uint32_t N>
	class Component
	{
	private: // subtypes
		using MyBase		= Component<CompositeT, ComponentT, N>;
		using MyComposite	= Composite<CompositeT, ComponentT, N>;

	public: // relations
		friend MyComposite;

	public: // subtypes
		class Index
		{
		public:		friend MyComposite;
		private:	Index(const uint32_t INDEX) : index(INDEX){}
		public:		ReadOnly<uint32_t, Index> index;
		};

		static const uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

	public: // data
		ReadOnly<uint32_t, Component> index;

	protected: // lifecycle
		CLASS_CTOR					Component()
			: index(INVALID_INDEX)
		{

		}

		CLASS_CTOR					Component(	const Index&		INDEX)
			: index(INDEX.index())
		{

		}

		/*
			WARNING! Do not move components manually! Move constructor can only be used by the Composite class.
		*/
		CLASS_CTOR					Component(	Component&&			other) noexcept
			: index(other.index())
		{

		}

		CLASS_DTOR					~Component() = default;

		inline Component&			operator=(	Component&&			other) noexcept
		{
			return *this; // Index does not change on move.
		}

		inline Component&			operator=(	Swap<Component>		other)
		{
			return *this; // Index does not change on swap.
		}

		inline Component&			operator=(	Swap<ComponentT>	other)
		{
			return *this; // Index does not change on swap.
		}

	private: // lifecycle
		CLASS_CTOR					Component(	const Component&	OTHER) = delete;

		Component&					operator=(	const Component&	OTHER) = delete;

	public: // functions
		/*
			Returns true if this component is part of the composite.
		*/
		inline bool					is_attached() const
		{
			return index() != INVALID_INDEX;
		}

		inline const CompositeT*	get_composite() const
		{
			return is_attached() ? reinterpret_cast<const MyComposite*>(convert_offset())->cast() : nullptr;
		}

	protected: // functions
		inline CompositeT*			get_composite()
		{
			return is_attached() ? reinterpret_cast<MyComposite*>(convert_offset())->cast() : nullptr;
		}

	private: // functions
		/*
			This function applies offset to 'this' pointer to get address of the composite and converts it to unsigned integer.
			We cannot use reinterpret_cast directly, because compiler applies its own weird offsets that give invalid address.
		*/
		inline uint64_t				convert_offset() const
		{
			return (uint64_t)(this->cast() - index());
		}

		inline ComponentT*			cast()
		{
			return static_cast<ComponentT*>(this);
		}

		inline const ComponentT*	cast() const
		{
			return static_cast<const ComponentT*>(this);
		}
	};


	template<typename CompositeT, typename ComponentT, uint32_t N>
	class Composite
	{
	private: // subtypes
		using MyBase		= Composite<CompositeT, ComponentT, N>;
		using MyComponent	= Component<CompositeT, ComponentT, N>;
		using MyComponents	= std::array<ComponentT, N>;
		using MyIndex		= typename MyComponent::Index;

		template<uint32_t... ints>
		using Indices		= std::integer_sequence<uint32_t, ints...>;

	public: // relations
		friend MyComponent;

	private: // data
		MyComponents components;

	protected: // lifecycle
		CLASS_CTOR					Composite()
			: Composite(std::make_integer_sequence<uint32_t, N>{})
		{

		}

		CLASS_CTOR					Composite(			Composite&&											other) noexcept
			: Composite(std::move(other), std::make_integer_sequence<uint32_t, N>{})
		{

		}

		CLASS_DTOR					~Composite() = default;

		inline Composite&			operator=(			Composite&&											other) noexcept
		{
			[&]<uint32_t... ints>(const Indices<ints...>){ ((components[ints] = std::move(other.components[ints])), ...); }();
			return *this;
		}

		inline Composite&			operator=(			Swap<Composite>										other)
		{
			for(uint32_t index = 0; index < N; ++index)
			{
				components[index] = Swap(other->components[index]);
			}
			return *this;
		}

		inline Composite&			operator=(			Swap<CompositeT>									other)
		{
			return operator=(Swap<Composite>(*other));
		}

	private: // lifecycle
		/*
			Source: https://jgreitemann.github.io/2018/09/15/variadic-expansion-in-aggregate-initialization/
		*/
		template<uint32_t... ints>
		CLASS_CTOR					Composite(			const Indices<ints...>								UINT_SEQUENCE)
			: components{MyIndex(ints)...}
		{
			
		}

		template<uint32_t... ints>
		CLASS_CTOR					Composite(			Composite&&											other,
														const Indices<ints...>								UINT_SEQUENCE)
			: components{std::move(other.components[ints])...}
		{
			
		}

		CLASS_CTOR					Composite(			const Composite&									OTHER) = delete;

		Composite&					operator=(			const Composite&									OTHER) = delete;

	public: // functions
		inline uint32_t				size() const
		{
			return N;
		}

		inline const ComponentT&	at(					const uint32_t										INDEX) const
		{
			validate_at(INDEX);
			return components[INDEX];
		}

		inline void					for_each_component(	std::function<void(const ComponentT&)>				FUNCTION) const
		{
			for(uint32_t index = 0; index < size(); ++index)
			{
				FUNCTION(components[index]);
			}
		}

		inline void					for_each_component(	std::function<void(const ComponentT&, uint32_t)>	FUNCTION) const
		{
			for(uint32_t index = 0; index < size(); ++index)
			{
				FUNCTION(components[index], index);
			}
		}

	protected: // functions
		inline ComponentT&			at(					const uint32_t										INDEX)
		{
			validate_at(INDEX);
			return components[INDEX];
		}

		inline void					for_each_component(	std::function<void(ComponentT&)>					function)
		{
			for(uint32_t index = 0; index < size(); ++index)
			{
				function(components[index]);
			}
		}

		inline void					for_each_component(	std::function<void(ComponentT&, uint32_t)>			function)
		{
			for(uint32_t index = 0; index < size(); ++index)
			{
				function(components[index], index);
			}
		}

	private: // functions
		inline CompositeT*			cast()
		{
			return static_cast<CompositeT*>(this);
		}

		inline const CompositeT*	cast() const
		{
			return static_cast<const CompositeT*>(this);
		}

		inline void					validate_at(		const uint32_t										INDEX) const
		{
#ifdef _DEBUG
			if(INDEX >= N)
				throw std::out_of_range("Composite index out of range.");
#endif // _DEBUG
		}
	};
}

#pragma pack(pop)