#include "..//include/complex_Instances.h"
#include "..//include/complex_Objects.h"
#include <dpl_Logger.h>

// InstancePack::AttachmentOperation
namespace complex
{
	void	InstanceRow::AttachmentOperation::attach(	InstanceRow& pack)
	{
		auto& group = InstanceManager::ref().get_group(groupName);
		if(complex::ss_size(state->binary) != 0)
		{
			state->reset_in();
			group.attach_instance_pack(pack, &state->binary);
		}
		else
		{
			group.attach_instance_pack(pack, nullptr);
		}
	}

	void	InstanceRow::AttachmentOperation::detach(	InstanceRow& pack)
	{
		auto& group = InstanceManager::ref().get_group(groupName);
		state->reset_out();
		group.detach_instance_pack(pack, &state->binary);
	}
}

// InstancePack::AttachCommand
namespace complex
{
	CLASS_CTOR	InstanceRow::AttachCommand::AttachCommand(	const InstanceGroup&	GROUP,
															const InstanceRow&		PACK)
		: AttachCommand(GROUP.get_label(), PACK.query_handle())
	{

	}

	bool		InstanceRow::AttachCommand::valid() const
	{
		if(!InstanceManager::ref().find_group(groupName)) return dpl::Logger::ref().push_error("[Fail to attach] Instance group could not be found: %s", groupName().c_str());
		const auto* PACK = ObjectManager::ref().find_instance_pack(packHandle);
		if(!PACK) return dpl::Logger::ref().push_error("[Fail to attach] Instance pack could not be found: %s", packHandle().packName.c_str());
		if(PACK->is_linked()) return dpl::Logger::ref().push_error("[Fail to attach] Instance pack already attached: %s", packHandle().packName.c_str());
		return true;
	}

	void		InstanceRow::AttachCommand::execute()
	{
		AttachmentOperation::attach(ObjectManager::ref().get_instance_pack(packHandle));
	}

	void		InstanceRow::AttachCommand::unexecute()
	{
		AttachmentOperation::detach(ObjectManager::ref().get_instance_pack(packHandle));
	}
}

// InstancePack::DetachCommand
namespace complex
{
	CLASS_CTOR	InstanceRow::DetachCommand::DetachCommand(	const InstanceRow&		PACK)
		: AttachmentOperation(PACK.is_linked()? PACK.get_group()->get_label() : "")
	{
		packHandle	= PACK.query_handle();
	}

	bool		InstanceRow::DetachCommand::valid() const
	{
		const auto* GROUP = InstanceManager::ref().find_group(groupName);
		if(!GROUP) return dpl::Logger::ref().push_error("[Fail to detach] Instance group could not be found: %s", groupName().c_str());
		const auto* PACK = ObjectManager::ref().find_instance_pack(packHandle);
		if(!PACK) return dpl::Logger::ref().push_error("[Fail to detach] Instance pack could not be found: %s", packHandle().packName.c_str());
		if(!PACK->is_linked(*GROUP)) return dpl::Logger::ref().push_error("[Fail to detach] Instance pack is not attached: %s", packHandle().packName.c_str());
		return true;
	}

	void		InstanceRow::DetachCommand::execute()
	{
		AttachmentOperation::detach(ObjectManager::ref().get_instance_pack(packHandle));
	}

	void		InstanceRow::DetachCommand::unexecute()
	{
		AttachmentOperation::attach(ObjectManager::ref().get_instance_pack(packHandle));
	}
}

// InstanceGroup
namespace complex
{
	CLASS_CTOR		InstanceGroup::InstanceGroup(			const dpl::Ownership&		OWNERSHIP)
		: Property(OWNERSHIP)
	{
		InstanceManager::ref().m_groupLabeler.label(*this, "group_");
	}

	CLASS_CTOR		InstanceGroup::InstanceGroup(			const dpl::Ownership&		OWNERSHIP, 
															InstanceGroup&&				other) noexcept
		: Property(OWNERSHIP, std::move(other))
		, Chain(std::move(other))
		, Labelable(std::move(other))
	{
			
	}

	InstanceGroup&	InstanceGroup::operator=(				dpl::Swap<InstanceGroup>	other)
	{
		Chain::operator=(dpl::Swap<Chain>(*other));
		Labelable::operator=(dpl::Swap<Labelable>(*other));
		return *this;
	}

	void			InstanceGroup::attach_instance_pack(	InstanceRow&				pack,
															std::istream*				binaryState)
	{
		if(pack.is_linked()) throw dpl::GeneralException(this, __LINE__, "Given pack is already attached: " + pack.query_handle().packName);
		if(Chain::attach_back(pack))
		{
			if(binaryState)
			{
				pack.import_all_instances_from(*binaryState);
			}
			else // initialize
			{
				pack.add_instances(query_numInstances());
			}
		}
	}

	void			InstanceGroup::detach_instance_pack(	InstanceRow&				pack,
															std::ostream*				binaryState)
	{
		if(!pack.is_linked(*this)) throw dpl::GeneralException(this, __LINE__, "Fail to detach. Unknown pack: " + pack.query_handle().packName);

		if(binaryState)
		{
			pack.export_all_instances_to(*binaryState);
		}
		else
		{
			pack.pop_instances(query_numInstances());
		}
	
		pack.detach();
	}

	void			InstanceGroup::add_instances(			const uint32_t				NUM_INSTANCES)
	{
		Chain::for_each([&](InstanceRow& pack)
		{
			pack.add_instances(NUM_INSTANCES);
		});
	}

	void			InstanceGroup::pop_instances(			const uint32_t				NUM_INSTANCES)
	{
		Chain::for_each([&](InstanceRow& pack)
		{
			pack.pop_instances(NUM_INSTANCES);
		});
	}

	void			InstanceGroup::swap_instances(			const uint32_t				FIRST_INDEX,
															const uint32_t				SECOND_INDEX)
	{
		if(FIRST_INDEX == SECOND_INDEX) return;
		Chain::for_each([&](InstanceRow& pack)
		{
			pack.swap_instances(FIRST_INDEX, SECOND_INDEX);
		});
	}

	void			InstanceGroup::import_all_from(			std::istream&				binaryState)
	{
		const auto				NUM_PACKS = dpl::import_t<uint32_t>(binaryState);
		InstanceRow::Handle	packHandle;

		for(uint32_t index = 0; index < NUM_PACKS; ++index)
		{
			packHandle.import_from(binaryState);
			InstanceRow* pack = ObjectManager::ref().find_instance_pack(packHandle);
			if(!pack) throw dpl::GeneralException(this, __LINE__, "Could not find given pack: " + packHandle.packName);
			attach_instance_pack(*pack, &binaryState);
		}
	}

	void			InstanceGroup::import_tail_from(		std::istream&				binaryState)
	{
		Chain::for_each([&](InstanceRow& pack)
		{
			pack.import_tail_instances_from(binaryState);
		});
	}

	void			InstanceGroup::export_all_to(			std::ostream&				binaryState)
	{
		dpl::export_t(binaryState, Chain::size());
		Chain::for_each([&](const InstanceRow& PACK)
		{
			PACK.query_handle().export_to(binaryState);
			PACK.export_all_instances_to(binaryState);
		});
	}

	void			InstanceGroup::export_tail_to(			std::ostream&				binaryState,
															const uint32_t				NUM_NEW_INSTANCES)
	{
		Chain::for_each([&](InstanceRow& pack)
		{
			pack.export_tail_instances_to(binaryState, NUM_NEW_INSTANCES);
		});
	}
}

// InstanceGroup::CreateCommand
namespace complex
{
	void	InstanceGroup::CreateCommand::execute()
	{
		InstanceManager::ref().push_group([&](InstanceGroup& group)
		{
			if(name().empty())
			{
				name = group.get_label();
			}
			else // Restore name
			{
				group.change_label(name); //<-- May fail if auto-generated name is the same.
			}
		});
	}

	void	InstanceGroup::CreateCommand::unexecute()
	{
		InstanceManager::ref().pull_group(name, nullptr);
	}
}

// InstanceGroup::DestroyCommand
namespace complex
{
	bool	InstanceGroup::DestroyCommand::valid() const
	{
		if(!InstanceManager::ref().find_group(m_name)) return dpl::Logger::ref().push_error("[Fail to destroy] Instance group could not be found: %s", m_name.c_str());
		return true;
	}

	void	InstanceGroup::DestroyCommand::execute()
	{
		InstanceManager::ref().pull_group(m_name, [&](InstanceGroup& group)
		{
			m_state.reset_out();
			group.export_all_to(m_state.binary);
		});
	}

	void	InstanceGroup::DestroyCommand::unexecute()
	{
		InstanceManager::ref().push_group([&](InstanceGroup& group)
		{
			m_state.reset_in();
			group.change_label(m_name);
			group.import_all_from(m_state.binary);
		});
	}
}

// InstanceGroup::RenameCommand
namespace complex
{
	bool	InstanceGroup::RenameCommand::valid() const
	{
		if(!InstanceManager::ref().find_group(m_oldName)) return dpl::Logger::ref().push_error("[Fail to rename] Instance group could not be found: %s", m_oldName.c_str());
		if(InstanceManager::ref().find_group(m_newName)) return dpl::Logger::ref().push_error("[Fail to rename] Instance group with given name already exists: %s", m_newName.c_str());
		return true;
	}

	void	InstanceGroup::RenameCommand::execute()
	{
		auto&	group = InstanceManager::ref().get_group(m_oldName);
				group.change_label(m_newName);
	}

	void	InstanceGroup::RenameCommand::unexecute()
	{
		auto&	group = InstanceManager::ref().get_group(m_newName);
				group.change_label(m_oldName);
	}
}

// InstanceGroup::EnlargeCommand
namespace complex
{
	bool	InstanceGroup::EnlargeCommand::valid() const
	{
		if(!InstanceManager::ref().find_group(groupName)) return dpl::Logger::ref().push_error("[Fail to enlarge] Instance group could not be found: %s", groupName().c_str());
		return amount() > 0;
	}

	void	InstanceGroup::EnlargeCommand::execute()
	{
		auto& group = InstanceManager::ref().get_group(groupName);

		if(complex::ss_size(state->binary) != 0)
		{
			state->reset_in();
			group.import_tail_from(state->binary);
		}
		else
		{
			group.add_instances(amount);
		}
	}

	void	InstanceGroup::EnlargeCommand::unexecute()
	{
		state->reset_out();
		auto&	group = InstanceManager::ref().get_group(groupName);
				group.export_tail_to(state->binary, amount);
				group.pop_instances(amount);
	}
}

// InstanceGroup::ReduceCommand
namespace complex
{
	bool	InstanceGroup::ReduceCommand::valid() const
	{
		if(!InstanceManager::ref().find_group(groupName)) return dpl::Logger::ref().push_error("[Fail to reduce] Instance group could not be found: %s", groupName().c_str());
		return amount() > 0;
	}

	void	InstanceGroup::ReduceCommand::execute()
	{
		state->reset_out();
		auto& group = InstanceManager::ref().get_group(groupName);
		const uint32_t NUM_INSTANCES = group.query_numInstances();
		if(amount() > NUM_INSTANCES) amount = NUM_INSTANCES;
		group.export_tail_to(state->binary, amount);
		group.pop_instances(amount);
	}

	void	InstanceGroup::ReduceCommand::unexecute()
	{
		auto&	group = InstanceManager::ref().get_group(groupName);

		if(complex::ss_size(state->binary) != 0)
		{
			state->reset_in();
			group.import_tail_from(state->binary);
		}
		else // This should never happen!
		{
			group.add_instances(amount);
		}
	}
}

// InstanceGroup::SwapInstancesCommand
namespace complex
{
	bool	InstanceGroup::SwapInstancesCommand::valid() const
	{
		const auto* GROUP = InstanceManager::ref().find_group(groupName);
		if(!GROUP) return dpl::Logger::ref().push_error("[Fail to swap instances] Instance group could not be found: %s", groupName().c_str());
		const uint32_t NUM_INSTANCES = GROUP->query_numInstances();
		if(firstIndex() >= NUM_INSTANCES) return dpl::Logger::ref().push_error("[Fail to swap instances] First index is out of bounds: %d", firstIndex());
		if(secondIndex() >= NUM_INSTANCES) return dpl::Logger::ref().push_error("[Fail to swap instances] Second index is out of bounds: %d", secondIndex());
		if(firstIndex() != secondIndex()) return dpl::Logger::ref().push_error("[Fail to swap instances] Both indices have the same index: %d", firstIndex());
		return true;
	}

	void	InstanceGroup::SwapInstancesCommand::execute()
	{
		auto&	group = InstanceManager::ref().get_group(groupName);
				group.swap_instances(firstIndex(), secondIndex());
	}

	void	InstanceGroup::SwapInstancesCommand::unexecute()
	{
		auto&	group = InstanceManager::ref().get_group(groupName);
				group.swap_instances(firstIndex(), secondIndex());
	}
}

// InstanceGroup::DestroyInstanceCommand
namespace complex
{
	bool	InstanceGroup::DestroyInstanceCommand::valid() const
	{
		const auto* GROUP = InstanceManager::ref().find_group(groupName);
		if(!GROUP) return dpl::Logger::ref().push_error("[Fail to destroy instance] Instance group could not be found: %s", groupName().c_str());
		if(instanceIndex() >= GROUP->query_numInstances()) return dpl::Logger::ref().push_error("[Fail to destroy instance] Index out of bounds: %d", instanceIndex());
		return true;
	}

	void	InstanceGroup::DestroyInstanceCommand::execute()
	{
		state->reset_out();
		auto&	group = InstanceManager::ref().get_group(groupName);
				group.swap_instances(instanceIndex(), group.query_numInstances()-1);
				group.export_tail_to(state->binary, 1);
				group.pop_instances(1);
	}

	void	InstanceGroup::DestroyInstanceCommand::unexecute()
	{
		auto&	group = InstanceManager::ref().get_group(groupName);

		if(complex::ss_size(state->binary) != 0)
		{
			state->reset_in();
			group.import_tail_from(state->binary);
		}
		else // This should never happen!
		{
			group.add_instances(1);
		}

		group.swap_instances(instanceIndex(), group.query_numInstances()-1);
	}
}