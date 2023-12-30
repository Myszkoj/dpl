#pragma once


#include "dpl_TypeTraits.h"
#include "dpl_Stream.h"
#include "dpl_Singleton.h"
#include "dpl_Variation.h"
#include "dpl_ThreadPool.h"


// concepts
namespace dpl
{
	template<typename ComponentT>
	concept is_Component			=  std::is_default_constructible_v<ComponentT>
									&& std::is_copy_assignable_v<ComponentT>;

	template<typename ComponentT>
	concept is_StreamableComponent	=  is_Streamable<ComponentT>
									&& is_Component<ComponentT>;

	template<typename ComponentT>
	struct	IsComponent
	{
		static const bool value = is_Component<ComponentT>;
	};

	template<typename COMPONENT_TYPES>
	concept	is_ComponentTypeList	=  dpl::is_TypeList<COMPONENT_TYPES> 
									&& COMPONENT_TYPES::ALL_UNIQUE 
									&& COMPONENT_TYPES::template all<IsComponent>();
}

// queries
namespace dpl
{
	template<typename T, bool IS_STREAMABLE>
	struct	ArrayQuery;

	template<typename T>
	struct	ArrayQuery<T, true>
	{
		static const bool IS_STREAMABLE = true;
		using Type = dpl::StreamChunk<T>;
	};

	template<typename T>
	struct	ArrayQuery<T, false>
	{
		static const bool IS_STREAMABLE = false;
		using Type = dpl::DynamicArray<T>;
	};
}

// declarations
namespace dpl
{
	template<typename CompositeT, is_ComponentTypeList COMPONENT_TYPES>
	class	ComponentTable;

	class	ComponentStream;

	template<is_StreamableComponent T>
	class	ComponentStream_of;

	template<is_StreamableComponent T, std::derived_from<Stream<T>> S>
	class	ComponentStreamImplementation_of;

	class	ComponentManager;
}

// definitions
namespace dpl
{
	template<typename CompositeT, typename... ComponentTn>
	class	ComponentTable<CompositeT, dpl::TypeList<ComponentTn...>>
	{
	public:		// [SUBTYPES]
		using	COMPONENT_TYPES = dpl::TypeList<ComponentTn...>;
		using	Column			= std::tuple<ComponentTn*...>;
		using	ConstColumn		= std::tuple<const ComponentTn*...>;

		template<is_Component T>
		using	ColumnStorage	= typename ArrayQuery<T, is_StreamableComponent<T>>::Type;

		template<is_Component T>
		class	Row	: private ColumnStorage<T>
		{
		private:	// [SUBTYPES]
			using	MyStorageBase	= ColumnStorage<T>;

		public:		// [SUBTYPES]
			using	Invoke		= typename ColumnStorage<T>::Invoke;
			using	InvokeConst	= typename ColumnStorage<T>::InvokeConst;

		public:		// [CONSTANTS]
			static constexpr bool IS_STREAMABLE = ArrayQuery<T, is_StreamableComponent<T>>::IS_STREAMABLE;

		public:		// [FRIENDS]
			friend	MyStorageBase;
			friend	ComponentTable;

		public:		// [EXPOSED FUNCTIONS]
			using	MyStorageBase::size;
			using	MyStorageBase::index_of;

		public:		// [LIFECYCLE]
			CLASS_CTOR			Row() = default;
			CLASS_CTOR			Row(				Row&&					other) noexcept = default;
			Row&				operator=(			Row&&					other) noexcept = default;

		private:	// [LIFECYCLE] (deleted)
			CLASS_CTOR			Row(				const Row&				OTHER) = delete;
			Row&				operator=(			const Row&				OTHER) = delete;

		public:		// [FUNCTIONS]
			uint32_t			offset() const
			{
				if constexpr(IS_STREAMABLE)	return MyStorageBase::offset;
				else							return 0;
			}

			T*					modify()
			{
				if constexpr(IS_STREAMABLE)	return MyStorageBase::modify();
				else							return MyStorageBase::data();
			}

			const T*			read() const
			{
				if constexpr(IS_STREAMABLE)	return MyStorageBase::read();
				else							return MyStorageBase::data();
			}

			void				modify_each(		const Invoke&		INVOKE)
			{
				if constexpr(IS_STREAMABLE)	return MyStorageBase::modify_each(INVOKE);
				else							return MyStorageBase::for_each(INVOKE);
			}

			void				read_each(			const InvokeConst&	INVOKE) const
			{
				if constexpr(IS_STREAMABLE)	return MyStorageBase::read_each(INVOKE);
				else							return MyStorageBase::for_each(INVOKE);
			}

			T*					at(					const uint32_t			COLUMN_INDEX)
			{
				return modify() + COLUMN_INDEX;
			}

			const T*			at(					const uint32_t			COLUMN_INDEX) const
			{
				return read() + COLUMN_INDEX;
			}

			uint32_t			index_of(			const T*				COMPONENT_ADDRESS) const
			{
				return MyStorageBase::index_of(COMPONENT_ADDRESS);
			}

		private:	// [INTERNAL FUNCTIONS]
			T*					enlarge(			const uint32_t			NUM_COLUMNS)
			{
				return MyStorageBase::enlarge(NUM_COLUMNS);
			}

			void				destroy_at(			const uint32_t			COLUMN_INDEX)
			{
				MyStorageBase::fast_erase(COLUMN_INDEX);
			}
		};

		using	Rows			= std::tuple<Row<ComponentTn>...>;

	private:	// [DATA]
		Rows m_rows; //<-- Each row has the same size.

	protected:	// [LIFECYCLE]
		CLASS_CTOR					ComponentTable() = default;

	public:		// [ROW FUNCTIONS]
		static constexpr uint32_t	numRows()
		{
			return COMPONENT_TYPES::SIZE;
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		Row<T>&						row()
		{
			return std::get<Row<T>>(m_rows);
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		const Row<T>&				row() const
		{
			return std::get<Row<T>>(m_rows);
		}

	public:		// [COLUMN FUNCTIONS]
		uint32_t					numColumns() const
		{
			using FirstComponentType = COMPONENT_TYPES::template At<0>;
			return Row<FirstComponentType>::size();
		}

		Column						column(			const uint32_t		COLUMN_INDEX)
		{
			return std::make_tuple(ComponentTable::row<ComponentTn>().at(COLUMN_INDEX)...);
		}

		ConstColumn					column(			const uint32_t		COLUMN_INDEX) const
		{
			return std::make_tuple(ComponentTable::row<ComponentTn>().at(COLUMN_INDEX)...);
		}

		template<dpl::is_one_of<COMPONENT_TYPES> T>
		uint32_t					column_index(	const T*			COMPONENT_ADDRESS) const
		{
			return ComponentTable::row<T>().index_of(COMPONENT_ADDRESS);
		}

	protected:	// [COLUMN FUNCTIONS]
		Column						add_columns(	const uint32_t		NUM_COLUMNS)
		{
			return std::make_tuple(ComponentTable::row<ComponentTn>().enlarge(NUM_COLUMNS)...);
		}

		Column						add_column()
		{
			return ComponentTable::add_columns(1);
		}
			
		void						remove_column(	const uint32_t		COLUMN_INDEX)
		{
			(ComponentTable::row<ComponentTn>().destroy_at(COLUMN_INDEX), ...);
		}
	};


	class	ComponentStream : private Variant<ComponentManager, ComponentStream>
	{
	public:		// [FRIENDS]
		friend ComponentManager;

		template<is_StreamableComponent>
		friend class ComponentStream_of;

	private:	// [LIFECYCLE]
		CLASS_CTOR		ComponentStream() = default;

	public:		// [LIFECYCLE]
		CLASS_DTOR		~ComponentStream() = default;

	public:		// [INTERFACE]
		// Call this function to update range at which chunks are stored (called automatically on sync, but may be useful in other scenarios). 
		virtual void	organize() = 0;

		// Call this function to make sure that components on the other side of the stream are synchronized.
		virtual void	sync() = 0;

		// Call this function every time you modify components on the other side of the stream.
		virtual void	unsync() = 0;
	};


	template<is_StreamableComponent T>
	class	ComponentStream_of : public ComponentStream
	{
	public:		// [INTERFACE]
		virtual void		organize() final override
		{
			base().organize();
		}

		virtual void		sync() final override
		{
			base().update();
		}

		virtual void		unsync() final override
		{
			base().notify_modified_remotely();
		}

	private:	// [INTERFACE]
		virtual Stream<T>&	base() = 0;
	};


	template<is_StreamableComponent T, std::derived_from<Stream<T>> ImplT>
	class	ComponentStreamImplementation_of : public ComponentStream_of<T>
											 , private ImplT
	{
	private:	// [INTERFACE]
		virtual Stream<T>&	base() final override
		{
			return *this;
		}
	};


	class	ComponentManager : public Singleton<ComponentManager>
							 , private Variation<ComponentManager, ComponentStream>
	{
	public:		// [FRIENDS]
		friend ComponentStream;

	public:		// [LIFECYCLE]
		CLASS_CTOR		ComponentManager(		dpl::Multition&		multition)
			: Singleton(multition)
		{

		}

		CLASS_DTOR		~ComponentManager()
		{
			dpl::no_except([&](){	destroy_all_streams();	});
		}

	public:		// [FUNCTIONS]
		// TODO: T should not be necessary, should be deduced from ImplT.
		template<is_StreamableComponent T, std::derived_from<Stream<T>> ImplT>
		bool			create_stream()
		{
			return Variation::create_variant<ComponentStreamImplementation_of<T, ImplT>>();
		}

		bool			destroy_all_streams()
		{
			return Variation::destroy_all_variants();
		}
	};
}