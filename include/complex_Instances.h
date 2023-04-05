#pragma once


#include <dpl_Singleton.h>
#include <dpl_Ownership.h>
#include <dpl_Chain.h>
#include <dpl_Labelable.h>
#include <dpl_Command.h>
#include <dpl_Stream.h>
#include "complex_Utilities.h"


// declarations
namespace complex
{
	/*
		Interface class for instance storage.
	*/
	class	InstanceRow;

	/*
		Constrols size and order of each linked InstancePack.
	*/
	class	InstanceGroup;

	/*
		Stores instance groups.
	*/
	class	InstanceManager;

	/*
		Specialize for EntityT to enable instantiation.
	*/
	template<typename EntityT>
	class	Instance_of;


	template<typename EntityT>
	concept is_Instance_of				= dpl::is_type_complete_v<Instance_of<EntityT>>;

	template<typename EntityT>
	concept is_TransferableInstance_of	= is_divisible_by<4, Instance_of<EntityT>>
										&& std::is_trivially_destructible_v<Instance_of<EntityT>>;

	/*
		Stores instances of the EntityT.
	*/
	template<is_Instance_of EntityT>
	class	InstancePack_of;
}

// implementations
namespace complex
{
	class	InstanceRow : private dpl::Link<InstanceGroup, InstanceRow>
	{
	private: // subtypes
		using	MyLink	= dpl::Link<InstanceGroup, InstanceRow>;

		struct	State
		{
			std::stringstream	binary;

			inline void reset_in()
			{
				binary.seekg(0);
				binary.clear();
			}

			inline void reset_out()
			{
				binary.str({}); //<-- Remove old state.
				binary.seekp(0);
				binary.clear();
			}
		};

	public: // subtypes
		class	Handle
		{
		public: // data
			std::string packName;
			std::string	typeName;

		public: // functions
			inline void import_from(std::istream& binary)
			{
				dpl::import_dynamic_container(binary, packName);
				dpl::import_dynamic_container(binary, typeName);
			}

			inline void export_to(	std::ostream& binary) const
			{
				dpl::export_container(binary, packName);
				dpl::export_container(binary, typeName);
			}
		};

	public: // friends
		friend	MyLink;
		friend	MyGroup;
		friend	MyBase;
		friend	InstanceGroup;

		template<is_Instance_of>
		friend class InstancePack_of;

	public: // states
		class	AttachmentOperation
		{
		public: //data
			dpl::ReadOnly<std::string,	AttachmentOperation> groupName;
			dpl::ReadOnly<State,		AttachmentOperation> state;

		public: // lifecycle
			CLASS_CTOR	AttachmentOperation(const std::string&	GROUP_NAME)
				: groupName(GROUP_NAME)
			{

			}

		public: // functions
			void		attach(				InstanceRow&		pack);
			void		detach(				InstanceRow&		pack);
		};

	public: // commands

		/*
			Command that attaches given pack to the InstanceGroup.
		*/
		class	AttachCommand	: public dpl::Command
								, private AttachmentOperation
		{
		public: // data
			dpl::ReadOnly<Handle, AttachCommand> packHandle;

		public: // lifecycle
			CLASS_CTOR		AttachCommand(	const std::string&		GROUP_NAME,
											const Handle&			PACK_HANDLE)
				: AttachmentOperation(GROUP_NAME)
				, packHandle(PACK_HANDLE)
			{

			}

			CLASS_CTOR		AttachCommand(	const InstanceGroup&	GROUP,
											const InstanceRow&		PACK);

		private: // implementation
			virtual bool	valid() const final override;
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

		/*
			Command that detaches given pack from the InstanceGroup.
		*/
		class	DetachCommand	: public dpl::Command
								, private AttachmentOperation
		{
		public: // data
			dpl::ReadOnly<Handle, DetachCommand> packHandle;

		public: // lifecycle
			CLASS_CTOR		DetachCommand(	const std::string&		GROUP_NAME,
											const Handle&			PACK_HANDLE)
				: AttachmentOperation(GROUP_NAME)
				, packHandle(PACK_HANDLE)
			{

			}

			CLASS_CTOR		DetachCommand(	const InstanceRow&		PACK);

		private: // implementation
			virtual bool	valid() const final override;
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

	private: // lifecycle
		CLASS_CTOR					InstanceRow() = default;

	private: // internal interface
		virtual uint32_t			query_numInstances() const = 0;

		virtual Handle				query_handle() const = 0;

		virtual void				add_instances(				const uint32_t	NUM_INSTANCES) = 0;

		virtual void				pop_instances(				const uint32_t	NUM_INSTANCES) = 0;

		virtual void				swap_instances(				const uint32_t	FIRST_INDEX,
																const uint32_t	SECOND_INDEX) = 0;

		virtual void				import_all_instances_from(	std::istream&	binary) = 0;

		virtual void				import_tail_instances_from(	std::istream&	binary) = 0;

		virtual void				export_all_instances_to(	std::ostream&	binary) const = 0;

		virtual void				export_tail_instances_to(	std::ostream&	binary,
																const uint32_t	NUM_NEW_INSTANCES) const = 0;
	};


	class	InstanceGroup	: public dpl::Property<InstanceManager, InstanceGroup>
							, private dpl::Chain<InstanceGroup, InstanceRow>
							, private dpl::Labelable<char>
	{
	public: // friends
		friend	Chain;
		friend	MyLink;
		friend	MyBase;
		friend	InstanceRow;
		friend	InstanceManager;

	private: // subtypes
		using	State = InstanceRow::State;

	public: // commands
		class	CreateCommand : public dpl::Command
		{
		public: // data
			dpl::ReadOnly<std::string, CreateCommand>	name;

		private: // interface
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

		class	DestroyCommand : public dpl::Command
		{
		private: // data
			std::string	m_name;
			State		m_state;

		public: // lifecycle
			CLASS_CTOR		DestroyCommand(const std::string&		GROUP_NAME)
				: m_name(GROUP_NAME)
			{

			}

			CLASS_CTOR		DestroyCommand(const InstanceGroup&	GROUP)
				: DestroyCommand(GROUP.get_label())
			{

			}

		private: // interface
			virtual bool	valid() const;
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

		class	RenameCommand : public dpl::Command
		{
		private: // data
			std::string m_oldName;
			std::string m_newName;

		public: // lifecycle
			CLASS_CTOR		RenameCommand(	const std::string&		OLD_NAME,
											const std::string&		NEW_NAME)
				: m_oldName(OLD_NAME)
				, m_newName(NEW_NAME)
			{

			}

			CLASS_CTOR		RenameCommand(	const InstanceGroup&	GROUP,
											const std::string&		NEW_NAME)
				: RenameCommand(GROUP.get_label(), NEW_NAME)
			{

			}

		private: // implementation
			virtual bool	valid() const final override;
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

		class	EnlargeCommand : public dpl::Command
		{
		public: // data
			dpl::ReadOnly<std::string,	EnlargeCommand> groupName;
			dpl::ReadOnly<uint32_t,		EnlargeCommand> amount;
			dpl::ReadOnly<State,		EnlargeCommand> state;

		public: // lifecycle
			CLASS_CTOR		EnlargeCommand(	const std::string&		GROUP_NAME,
											const uint32_t			AMOUNT)
				: groupName(GROUP_NAME)
				, amount(AMOUNT)
			{

			}

			CLASS_CTOR		EnlargeCommand(	const InstanceGroup&	GROUP,
											const uint32_t			AMOUNT)
				: EnlargeCommand(GROUP.get_label(), AMOUNT)
			{

			}

		private: // implementation
			virtual bool	valid() const final override;
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

		class	ReduceCommand : public dpl::Command
		{
		public: // constants
			static const uint32_t ALL = std::numeric_limits<uint32_t>::max();

		public: // data
			dpl::ReadOnly<std::string,	ReduceCommand> groupName;
			dpl::ReadOnly<uint32_t,		ReduceCommand> amount;
			dpl::ReadOnly<State,		ReduceCommand> state;

		public: // lifecycle
			CLASS_CTOR		ReduceCommand(	const std::string&		GROUP_NAME,
											const uint32_t			AMOUNT = ALL)
				: groupName(GROUP_NAME)
				, amount(AMOUNT)
			{

			}

			CLASS_CTOR		ReduceCommand(	const InstanceGroup&	GROUP,
											const uint32_t			AMOUNT = ALL)
				: ReduceCommand(GROUP.get_label(), AMOUNT)
			{

			}

		private: // implementation
			virtual bool	valid() const final override;
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

		class	SwapInstancesCommand : public dpl::Command
		{
		public: // data
			dpl::ReadOnly<std::string,	SwapInstancesCommand> groupName;
			dpl::ReadOnly<uint32_t,		SwapInstancesCommand> firstIndex;
			dpl::ReadOnly<uint32_t,		SwapInstancesCommand> secondIndex;

		public: // lifecycle
			CLASS_CTOR		SwapInstancesCommand(	const std::string&		GROUP_NAME,
													const uint32_t			FIRST_INDEX,
													const uint32_t			SECOND_INDEX)
				: groupName(GROUP_NAME)
				, firstIndex(FIRST_INDEX)
				, secondIndex(SECOND_INDEX)
			{

			}

			CLASS_CTOR		SwapInstancesCommand(	const InstanceGroup&	GROUP,
													const uint32_t			FIRST_INDEX,
													const uint32_t			SECOND_INDEX)
				: SwapInstancesCommand(GROUP.get_label(), FIRST_INDEX, SECOND_INDEX)
			{

			}

		private: // implementation
			virtual bool	valid() const final override;
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

		class	DestroyInstanceCommand : public dpl::Command
		{
		public: // data
			dpl::ReadOnly<std::string,	DestroyInstanceCommand> groupName;
			dpl::ReadOnly<uint32_t,		DestroyInstanceCommand> instanceIndex;
			dpl::ReadOnly<State,		DestroyInstanceCommand> state;

		public: // lifecycle
			CLASS_CTOR		DestroyInstanceCommand(	const std::string&		GROUP_NAME,
													const uint32_t			INSTANCE_INDEX)
				: groupName(GROUP_NAME)
				, instanceIndex(INSTANCE_INDEX)
			{

			}

			CLASS_CTOR		DestroyInstanceCommand(	const InstanceGroup&	GROUP,
													const uint32_t			INSTANCE_INDEX)
				: DestroyInstanceCommand(GROUP.get_label(), INSTANCE_INDEX)
			{

			}

		private: // implementation
			virtual bool	valid() const final override;
			virtual void	execute() final override;
			virtual void	unexecute() final override;
		};

	public: // inherited functions
		using	Labelable::get_label;

	public: // lifecycle
		CLASS_CTOR		InstanceGroup(			const dpl::Ownership&		OWNERSHIP);

		CLASS_CTOR		InstanceGroup(			const dpl::Ownership&		OWNERSHIP, 
												InstanceGroup&&				other) noexcept;

		InstanceGroup&	operator=(				dpl::Swap<InstanceGroup>	other);

	public: // functions
		inline uint32_t query_numInstances() const
		{
			if(Chain::size() == 0) return 0;
			return Chain::first()->query_numInstances();
		}

	public: // command functions
		inline bool		rename(					dpl::CommandInvoker&		invoker,
												const std::string&			NEW_NAME) const
		{
			return invoker.invoke<RenameCommand>(*this, NEW_NAME);
		}

		inline bool		enlarge(				dpl::CommandInvoker&		invoker,
												const uint32_t				AMOUNT) const
		{
			return invoker.invoke<EnlargeCommand>(*this, AMOUNT);
		}

		inline bool		reduce(					dpl::CommandInvoker&		invoker,
												const uint32_t				AMOUNT = ReduceCommand::ALL) const
		{
			return invoker.invoke<ReduceCommand>(*this, AMOUNT);
		}

		inline bool		swap_instances(			dpl::CommandInvoker&		invoker,
												const uint32_t				FIRST_INDEX,
												const uint32_t				SECOND_INDEX) const
		{
			return invoker.invoke<SwapInstancesCommand>(*this, FIRST_INDEX, SECOND_INDEX);
		}

		inline bool		destroy_instance(		dpl::CommandInvoker&		invoker,
												const uint32_t				INSTANCE_INDEX) const
		{
			return invoker.invoke<DestroyInstanceCommand>(*this, INSTANCE_INDEX);
		}

	private: // functions
		void			attach_instance_pack(	InstanceRow&				pack,
												std::istream*				binaryState);

		void			detach_instance_pack(	InstanceRow&				pack,
												std::ostream*				binaryState);

	private: // functions
		void			add_instances(			const uint32_t				NUM_INSTANCES);

		void			pop_instances(			const uint32_t				NUM_INSTANCES);

		void			swap_instances(			const uint32_t				FIRST_INDEX,
												const uint32_t				SECOND_INDEX);

		void			import_all_from(		std::istream&				binaryState);

		void			import_tail_from(		std::istream&				binaryState);

		void			export_all_to(			std::ostream&				binaryState);

		void			export_tail_to(			std::ostream&				binaryState,
												const uint32_t				NUM_NEW_INSTANCES);
	};


	class	InstanceManager : public dpl::DynamicOwner<InstanceManager, InstanceGroup>
							, public dpl::Singleton<InstanceManager>
	{
	public: // friends
		friend	InstanceRow;
		friend	InstanceGroup;

	public: // commands
		using	InvokeGroup = std::function<void(InstanceGroup&)>;

	private: // data
		dpl::Labeler<char> m_groupLabeler;

	public: // functions
		inline InstanceGroup&		create_group(			dpl::CommandInvoker&		invoker)
		{
			if(!invoker.invoke<InstanceGroup::CreateCommand>()) throw dpl::GeneralException(this, __LINE__, "Fail to create instance group.");
			return DynamicOwner::get_last_property();
		}

		inline bool					destroy_all_groups(		dpl::CommandInvoker&		invoker)
		{
			return invoker.invoke<dpl::CommandPack>([&](dpl::CommandPack::Builder& builder)
			{
				DynamicOwner::invoke_properties_backward([&](const InstanceGroup& GROUP)
				{
					builder.add_command<InstanceGroup::DestroyCommand>(GROUP);
				});
			});
		}

		inline InstanceGroup*		find_group(				const std::string&			GROUP_NAME)
		{
			return static_cast<InstanceGroup*>(m_groupLabeler.find_entry(GROUP_NAME));
		}

		inline const InstanceGroup* find_group(				const std::string&			GROUP_NAME) const
		{
			return static_cast<const InstanceGroup*>(m_groupLabeler.find_entry(GROUP_NAME));
		}

		inline InstanceGroup&		get_group(				const std::string&			GROUP_NAME)
		{
			return *find_group(GROUP_NAME);
		}

		inline const InstanceGroup& get_group(				const std::string&			GROUP_NAME) const
		{
			return *find_group(GROUP_NAME);
		}

		inline InstanceGroup&		get_group(				const uint32_t				GROUP_INDEX)
		{
			return DynamicOwner::get_property_at(GROUP_INDEX);
		}

		inline const InstanceGroup& get_group(				const uint32_t				GROUP_INDEX) const
		{
			return DynamicOwner::get_property_at(GROUP_INDEX);
		}

	private: // functions
		void						pull_group(				const std::string&			GROUP_NAME,
															const InvokeGroup&			INVOKE)
		{
			auto* group = find_group(GROUP_NAME);
			if(!group) throw dpl::GeneralException(this, __LINE__, "Group is missing.");
			INVOKE(*group);
			DynamicOwner::destroy_property_at(group->get_index());
		}

		inline void					push_group(				const InvokeGroup&			INVOKE)
		{
			auto& newGroup = DynamicOwner::create_property();
			m_groupLabeler.label_with_postfix(newGroup, "InstanceGroup_");
			INVOKE(newGroup);
		}
	};


	template<is_Instance_of EntityT>
	class	InstancePack_of : public InstanceRow
	{
	private: // subtypes
		using	Instance_t		= Instance_of<EntityT>;
		using	InstanceStorage	= std::conditional_t<	is_TransferableInstance_of<EntityT>,
														dpl::StreamController<Instance_t>, 
														dpl::DynamicArray<Instance_t>>;

	public: // subtypes
		using	NumInstances	= uint32_t;
		using	InvokeAll		= std::function<void(Instance_t*, NumInstances)>;
		using	InvokeConstAll	= std::function<void(const Instance_t*, NumInstances)>;

	public: // constants
		static constexpr bool IS_TRANSFERABLE = std::is_same_v<InstanceStorage, dpl::StreamController<Instance_t>>;

	private: // data
		InstanceStorage m_instances;

	public: // functions
		inline Instance_t*			modify_instances()
		{
			return m_instances.modify();
		}

		inline const Instance_t*	read_instances() const
		{
			return m_instances.read();
		}

		inline void					invoke_instances(			const InvokeAll&		INVOKE)
		{
			if constexpr(IS_TRANSFERABLE)	INVOKE(m_instances.modify(), m_instances.size());
			else							INVOKE(m_instances.data(), m_instances.size());
		}

		inline void					invoke_instances(			const InvokeConstAll&	INVOKE) const
		{
			
			if constexpr(IS_TRANSFERABLE)	INVOKE(m_instances.read(), m_instances.size());
			else							INVOKE(m_instances.data(), m_instances.size());
		}

	private: // internal interface
		virtual uint32_t			query_numInstances() const final override
		{
			return m_instances.size();
		}

		virtual void				add_instances(				const uint32_t			NUM_INSTANCES) final override
		{
			return m_instances.enlarge(NUM_INSTANCES);
		}

		virtual void				pop_instances(				const uint32_t			NUM_INSTANCES) final override
		{
			m_instances.reduce(NUM_INSTANCES);
		}

		virtual void				swap_instances(				const uint32_t			FIRST_INDEX,
																const uint32_t			SECOND_INDEX) final override
		{
			m_instances.swap_elements(FIRST_INDEX, SECOND_INDEX);
		}

		virtual void				import_all_instances_from(	std::istream&			binary) final override
		{
			m_instances.import_from(binary);
		}

		virtual void				import_tail_instances_from(	std::istream&			binary) final override
		{
			m_instances.import_tail_from(binary);
		}

		virtual void				export_all_instances_to(	std::ostream&			binary) const final override
		{
			m_instances.export_to(binary);
		}

		virtual void				export_tail_instances_to(	std::ostream&			binary,
																const uint32_t			NUM_NEW_INSTANCES) const final override
		{
			m_instances.export_tail_to(binary, NUM_NEW_INSTANCES);
		}
	};
}