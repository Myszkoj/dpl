#pragma once


#include <functional>
#include "dpl_ReadOnly.h"
#include "dpl_Swap.h"
#include "dpl_Result.h"
#include "dpl_VirtualConstructable.h"


// declarations
namespace dpl
{
	template<typename VariationT, typename VariantT>
	class Variant;

	template<typename VariationT, typename VariantT>
	class Variation;
}

// definitions
namespace dpl
{
	template<typename VariationT, typename VariantT>
	class Variation
	{
	protected: // subtypes
		using MyType		= Variation<VariationT, VariantT>;
		using MyVariant		= Variant<VariationT, VariantT>;
		using MyVariants	= std::unordered_map<std::type_index, std::unique_ptr<MyVariant>>;
		using MyIterator	= typename MyVariants::iterator;

	public: // subtypes
		class	Binding
		{
		public: // relations
			friend MyType;

		private: // lifecycle
			CLASS_CTOR	Binding(MyType& owner)
				: owner(&owner)
			{

			}

		public: // data
			ReadOnly<MyType*, Binding> owner;
		};

		using	VariantHandler	= VirtualConstructable<MyVariant, const Binding&>;

	public: // relations
		friend	MyVariant;

	private: // data
		MyVariants m_variants;

	protected: // lifecycle
		CLASS_CTOR				Variation() = default;

		CLASS_CTOR				Variation(					const Variation&						OTHER) = delete;

		CLASS_CTOR				Variation(					Variation&&								other) noexcept
			: m_variants(std::move(other.m_variants))
		{
			bind_variants();
		}

		CLASS_DTOR				~Variation() = default;

		Variation&				operator=(					const Variation&						OTHER) = delete;

		Variation&				operator=(					Variation&&								other) noexcept
		{
			if(this != &other)
			{
				m_variants = std::move(other.m_variants);
				bind_variants();
			}

			return *this;
		}

		Variation&				operator=(					Swap<Variation>							other)
		{
			std::swap(m_variants, other->m_variants);
			bind_variants();
			other->bind_variants();
			return *this;
		}

		Variation&				operator=(					Swap<VariationT>						other)
		{
			return operator=(Swap<Variation>(*other));
		}

	public: // functions
		template<typename T>
		static uint32_t			get_typeID()
		{
			return VariantHandler::template get_typeID<T>();
		}

		uint32_t				get_numVariants() const
		{
			return static_cast<uint32_t>(m_variants.size());
		}

		VariantT*				find_base_variant(			const uint32_t							TYPE_ID)
		{
			auto it = m_variants.find(VariantHandler::get_typeInfo(TYPE_ID));
			return (it != m_variants.end()) ? static_cast<VariantT*>(it->second.get()) : nullptr;
		}

		const VariantT*			find_base_variant(			const uint32_t							TYPE_ID) const
		{
			auto it = m_variants.find(VariantHandler::get_typeInfo(TYPE_ID));
			return (it != m_variants.end()) ? static_cast<const VariantT*>(it->second.get()) : nullptr;
		}

		VariantT*				find_base_variant(			const std::string&						VARIANT_NAME)
		{
			return Variation::find_base_variant(VariantHandler::get_typeID(VARIANT_NAME));
		}

		const VariantT*			find_base_variant(			const std::string&						VARIANT_NAME) const
		{
			return Variation::find_base_variant(VariantHandler::get_typeID(VARIANT_NAME));
		}

		VariantT&				get_base_variant(			const uint32_t							TYPE_ID)
		{
			return *find_base_variant(TYPE_ID);
		}

		const VariantT&			get_base_variant(			const uint32_t							TYPE_ID) const
		{
			return *find_base_variant(TYPE_ID);
		}

		VariantT&				get_base_variant(			const std::string&						VARIANT_NAME)
		{
			return *find_base_variant(VARIANT_NAME);
		}

		const VariantT&			get_base_variant(			const std::string&						VARIANT_NAME) const
		{
			return *find_base_variant(VARIANT_NAME);
		}

		bool					has_variant(				const uint32_t							TYPE_ID) const
		{	
			return find_base_variant(TYPE_ID) != nullptr;
		}

		template<typename T>
		bool					has_variant() const
		{
			return m_variants.find(typeid(T)) != m_variants.end();
		}

		template<typename T>
		const T*				find_variant() const
		{
			auto it = m_variants.find(typeid(T));
			return (it != m_variants.end()) ? static_cast<const T*>(it->second.get()) : nullptr;
		}

		template<typename T>
		const T&				get_variant() const
		{
			return *find_variant<T>();
		}

	protected: // functions
		template<typename T>
		T*						find_variant()
		{
			auto it = m_variants.find(typeid(T));
			return (it != m_variants.end()) ? static_cast<T*>(it->second.get()) : nullptr;
		}

		template<typename T>
		T&						get_variant()
		{
			return *find_variant<T>();
		}

		Result<VariantT*>		create_default_variant(		const uint32_t							TYPE_ID)
		{
			auto result = m_variants.emplace(VariantHandler::get_typeInfo(TYPE_ID), nullptr);
			if(result.second)
			{
				std::unique_ptr<MyVariant>&	newVariant = result.first->second;
											newVariant = VariantHandler::generate_object(TYPE_ID, Binding(*this));

				return Result<VariantT*>(true, static_cast<VariantT*>(newVariant.get()));
			}

			return Result<VariantT*>(false, static_cast<VariantT*>(result.first->second.get()));
		}

		Result<VariantT*>		create_default_variant(		const std::string&						VARIANT_NAME)
		{
			return Variation::create_default_variant(VariantHandler::get_typeID(VARIANT_NAME));
		}

		template<typename T, typename... CTOR>
		Result<T*>				create_variant(				CTOR&&...								args)
		{
			VariantHandler::template register_generator<T>();
			auto result = m_variants.emplace(typeid(T), nullptr);
			if(result.second)
			{
				std::unique_ptr<MyVariant>&	newVariant = result.first->second;
											newVariant.reset(new T(Binding(*this), std::forward<CTOR>(args)...));

				return Result<T*>(true, static_cast<T*>(newVariant.get()));
			}

			return Result<T*>(false, static_cast<T*>(result.first->second.get()));
		}

		template<typename T>
		bool					destroy_variant()
		{
			return destroy_variant_internal(typeid(T));
		}

		bool					destroy_variant(			const uint32_t							TYPE_ID)
		{
			return destroy_variant_internal(VariantHandler::get_typeInfo(TYPE_ID));
		}

		// Returns false if T was not present.
		template<typename T>
		bool					destroy_all_variants_except()
		{
			bool bResult = false;
			auto it = m_variants.begin();
			while(it != m_variants.end())
			{
				if(it->first != typeid(T))
				{
					it = destroy_it_internal(it);
				}
				else
				{
					it = std::next(it);
					bResult = true;
				}
			}

			return bResult;
		}

		bool					destroy_all_variants()
		{
			if(m_variants.empty()) return false;
			while(!m_variants.empty())
			{
				auto it = m_variants.begin();
				destroy_it_internal(it);
			}
			return true;
		}

		void					iterate(					std::function<void(VariantT&)>			function)
		{
			for(auto& it : m_variants)
			{
				function(*it.second->cast());
			}
		}

		void					iterate(					std::function<void(const VariantT&)>	function) const
		{
			for(auto& it : m_variants)
			{
				function(*it.second->cast());
			}
		}

		void					for_each_variant(			std::function<void(VariantT&)>			function)
		{
			Variation::iterate(function);
		}

		void					for_each_variant(			std::function<void(const VariantT&)>	function) const
		{
			Variation::iterate(function);
		}

	private: // functions
		bool					destroy_variant_internal(	const std::type_index&					TYPE_INDEX)
		{
			auto it = m_variants.find(TYPE_INDEX);
			if(it != m_variants.end())
			{
				destroy_it_internal(it);		
				return true;
			}

			return false;
		}

		MyIterator				destroy_it_internal(		MyIterator&								iterator)
		{
			return m_variants.erase(iterator);
		}

	private: // functions
		VariationT*				cast()
		{
			return static_cast<VariationT*>(this);
		}

		const VariationT*		cast() const
		{
			return static_cast<const VariationT*>(this);
		}

		void					bind_variants()
		{
			for(auto& it : m_variants)
			{
				it.second->variation = this;
			}
		}
	};


	template<typename VariationT, typename VariantT>
	class Variant : public VirtualConstructable<Variant<VariationT, VariantT>, const typename Variation<VariationT, VariantT>::Binding&>
	{
	private: // subtypes
		using	MyVariation	= Variation<VariationT, VariantT>;

	public: // subtypes
		using	VCTOR_Base	= VirtualConstructable<Variant<VariationT, VariantT>, const typename Variation<VariationT, VariantT>::Binding&>;
		using	Binding		= typename MyVariation::Binding;

	public: // relations
		friend	MyVariation;
		friend	VariationT; //<-- Gives access to import_from/export_to functions.

	private: // data
		ReadOnly<MyVariation*,	MyVariation> variation;

	public: // lifecycle
		CLASS_DTOR virtual			~Variant() = default;

	protected: // lifecycle
		CLASS_CTOR					Variant(		const Binding&		BINDING)
			: variation(BINDING.owner())
		{

		}

		CLASS_CTOR					Variant(		const Variant&		OTHER) = delete;

		CLASS_CTOR					Variant(		Variant&&			other) noexcept = delete;

		Variant&					operator=(		const Variant&		OTHER) = delete;

		Variant&					operator=(		Variant&&			other) noexcept = delete;

	public: // functions
		inline bool					has_variation() const
		{
			return variation() != nullptr;
		}

		inline VariationT&			get_variation()
		{
			return *variation()->cast();
		}

		inline const VariationT&	get_variation() const
		{
			return *variation()->cast();
		}

		inline VariantT*			cast()
		{
			return static_cast<VariantT*>(this);
		}

		inline const VariantT*		cast() const
		{
			return static_cast<const VariantT*>(this);
		}

	protected: // functions
		inline bool					selfdestruct()
		{
			return this->has_variation() ? variation()->destroy_variant_internal(typeid(*this)) : false;
		}
	};
}