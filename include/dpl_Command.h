#pragma once


#include <vector>
#include <memory>
#include <stdint.h>
#include <limits>
#include <type_traits>
#include "dpl_Result.h"

#pragma warning( push )
#pragma warning( disable : 26451)

namespace dpl
{
	class CommandPack;
	class CommandInvoker;
	class CommandHistory;


	class Command
	{
	public: // friends
		friend CommandPack;
		friend CommandInvoker;
		friend CommandHistory;

	public: // functions
		CLASS_DTOR virtual	~Command(){}

	public: // interface
		virtual bool		valid() const
		{
			return true;
		}

		virtual void		execute() = 0;

		virtual void		unexecute() = 0;
	};


	class EmptyCommand : public Command
	{
	public: // interface
		virtual bool		valid() const final override
		{
			return true;
		}

		virtual void		execute() final override{}

		virtual void		unexecute() final override{}
	};


	template<typename CommandT>
	concept is_Command = std::is_base_of_v<Command, CommandT>;


	class CommandPack : public Command
	{
	private: // subtypes
		using MyCommands = std::vector<std::unique_ptr<Command>>;

	public: // functions
		class Builder
		{
		public: // friends
			friend CommandPack;

		private: // data
			MyCommands& m_commands;

		private: // lifecycle
			CLASS_CTOR	Builder(		MyCommands& commands)
				: m_commands(commands)
			{

			}

		public: // functions
			template<typename CommandT, typename... CTOR>
			CommandT*	add_command(	CTOR&&...	args)
			{
				auto newCommand = std::make_unique<CommandT>(std::forward<CTOR>(args)...);
				if(!is_valid(newCommand.get())) return nullptr;
				m_commands.emplace_back(std::move(newCommand));
				return static_cast<CommandT*>(m_commands.back().get());
			}

		private: // functions
			private: // functions
			inline bool	is_valid(		Command*	command) const
			{
				return command->valid();
			}
		};

	private: // data
		MyCommands		m_commands;

	public: // lifecycle
		CLASS_CTOR			CommandPack(const std::function<void(Builder&)>& INVOKE)
		{
			Builder builder(m_commands);
			if(INVOKE) INVOKE(builder);
		}

	private: // interface
		virtual bool		valid() const final override
		{
			return !m_commands.empty();
		}

		virtual void		execute() final override
		{
			for(std::unique_ptr<Command>& command : m_commands)
			{
				command->execute();
			}
		}

		virtual void		unexecute() final override
		{
			auto		iCommand	= m_commands.rbegin();
			const auto	END			= m_commands.rend();
			while(iCommand != END)
			{
				iCommand->get()->unexecute();
				++iCommand;
			}
		}
	};


	class CommandHistory
	{
	private: // subtypes
		using MyCommands	= std::vector<std::unique_ptr<Command>>;

	public: // subtypes
		static const uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

	private: // data
		MyCommands	m_commands;
		uint32_t	m_currentID;

	public: // lifecycle
		CLASS_CTOR			CommandHistory()
			: m_currentID(INVALID_INDEX)
		{

		}

	public: // functions
		/*
			Creates new command, invokes user function with it, adds it after current one and returns pointer to it.
		*/
		template<is_Command T, typename... CTOR>
		Command*			update(				CTOR&&...		args)
		{
			auto newCommand = std::make_unique<T>(std::forward<CTOR>(args)...);
			if(!is_valid(newCommand.get())) return nullptr;
			trim_to_current();	
			++m_currentID; // Note: If current is equal to INVALID_INDEX32 the value is winded back to the 0.
			return m_commands.emplace_back(std::move(newCommand)).get();
		}

		/*
			Returns current command and moves to the previous one.
		*/
		inline Command*		move_backward()
		{
			if(m_currentID == INVALID_INDEX) return nullptr;
			return m_commands[m_currentID--].get();
		}

		/*
			Moves to the next command and returns it as current.
		*/
		inline Command*		move_forward()
		{
			if(m_currentID >= get_lastID()) return nullptr;
			return m_commands[++m_currentID].get();
		}

		inline bool			clear()
		{
			if(m_commands.empty()) return false;
			m_commands.clear();
			m_currentID = INVALID_INDEX;
			return true;
		}

	private: // functions
		inline bool			is_valid(			Command*		command) const
		{
			return command->valid();
		}

		inline uint32_t		get_lastID() const
		{
			// Note that this value may wind up to INVALID_INDEX, which is fine, since m_currentID can also have this value.
			return (uint32_t)m_commands.size() - 1;
		}

		inline void			trim_to_current()
		{
			if(m_currentID < get_lastID()) 
				m_commands.resize(m_currentID+1u);	
		}
	};


	/*
		Keeps track of invoked commands(Memento pattern).
		Supports undo/redo operations.
	*/
	class CommandInvoker
	{
	private: // data
		CommandHistory	m_history;

	public: // functions
		// All calls to 'invoke' will be packed together into a single command.
		inline void		pack_commands(const std::function<void()>& INVOKE_COMMANDS)
		{
			if(INVOKE_COMMANDS)
			{
				// [TODO]: open pack
				INVOKE_COMMANDS();
				// [TODO]: close pack
			}
		}

		template<is_Command T, typename... CTOR>
		inline T*		invoke(	CTOR&&...	args)
		{
			Command* command = m_history.update<T>(std::forward<CTOR>(args)...);
			if(!command) return nullptr;
			command->execute();
			return static_cast<T*>(command);
		}

		/*
			Unexecutes current command and moves to the previous one.
		*/
		inline void		undo()
		{
			if(Command* command = m_history.move_backward())
			{
				command->unexecute();
			}
		}

		/*
			Invokes command after the current one.
		*/
		inline void		redo()
		{
			if(Command* command = m_history.move_forward())
			{
				command->execute();
			}
		}

		/*
			Clears command history.
		*/
		inline bool		clear()
		{
			return m_history.clear();
		}
	};
}

#pragma warning( pop )