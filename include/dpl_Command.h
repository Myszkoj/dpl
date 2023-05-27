#pragma once


#include <vector>
#include <memory>
#include <stdint.h>
#include <limits>
#include <sstream>
#include "dpl_Binary.h"
#include "dpl_Result.h"
#include "dpl_TypeTraits.h"
#include "dpl_Logger.h"

#pragma warning( push )
#pragma warning( disable : 26451)

// forward declarations
namespace dpl
{
	class BinaryState;
	class BinaryCommand;
	class BinaryInvoker;
}

// binary state implementation
namespace dpl
{
	class	BinaryState
	{
	public:		// [FRIENDS]
		friend BinaryCommand;
		friend BinaryInvoker;

	private:	// [DATA]
		dpl::ReadOnly<std::stringstream, BinaryState> file;

	private:	// [LIFECYCLE]
		CLASS_CTOR		BinaryState() = default;

	public:		// [FUNCTIONS]
		template<typename T>
		void			save(			const T&				DATA)
		{
			if constexpr (std::is_trivially_destructible_v<T>)
			{
				file->write(reinterpret_cast<const char*>(&DATA), sizeof(T));
			}
			else // Force operator
			{
				DATA >> *this;
			}
		}

		template<typename T>
		void			save(			const size_t			SIZE,
										const T*				DATA)
		{
			if constexpr (std::is_trivially_destructible_v<T>)
			{
				file->write(reinterpret_cast<const char*>(DATA), SIZE * sizeof(T));
			}
			else // Force operator
			{
				for(uint32_t index = 0; index < SIZE; ++index)
				{
					BinaryState::save<T>(DATA[index]);
				}
			}
		}

		template<is_container T>
		void			save(			const T&				CONTAINER)
		{
			const auto SIZE = CONTAINER.size();
			BinaryState::save(SIZE);
			BinaryState::save<typename T::value_type>(SIZE, CONTAINER.data());
		}

		template<typename T>
		void			load(			T&						data)
		{
			if constexpr (std::is_trivially_destructible_v<T>)
			{
				file->read(reinterpret_cast<char*>(&data), sizeof(T));
			}
			else // Force operator
			{
				data << *this;
			}
		}

		template<typename T>
		void			load(			const size_t			SIZE,
										T*						data)
		{
			if constexpr (std::is_trivially_destructible_v<T>)
			{
				file->read(reinterpret_cast<char*>(data), SIZE * sizeof(T));
			}
			else // Force operator
			{
				for(uint32_t index = 0; index < SIZE; ++index)
				{
					BinaryState::load<T>(data[index]);
				}
			}
		}

		template<typename T>
		T				load()
		{
			thread_local T variable;
			BinaryState::load<T>(variable);
			return variable;
		}

		template<is_container T>
		void			load(			T&						container)
		{
			const auto SIZE = BinaryState::load<typename T::size_type>();
			container.resize(SIZE);
			BinaryState::load<typename T::value_type>(SIZE, container.data());
		}

		template<template<typename, size_t> class Array, typename T, size_t N>
		void			load(			Array<T, N>&			container)
		{
			BinaryState::load<typename Array<T, N>::value_type>(BinaryState::load<typename Array<T, N>::size_type>(), container.data());
		}

	private:	// [COMMAND FUNCTIONS]
		void			seekg(			const std::streamoff&	OFFSET)
		{
			file->seekg(OFFSET);
		}

		void			seekp(			const std::streamoff&	OFFSET)
		{
			file->seekp(OFFSET);
		}

		std::streamoff	tellg()
		{
			return file->tellg();
		}

		std::streamoff	tellp()
		{
			return file->tellp();
		}
	};
}

// binary command implementation
namespace dpl
{
	class	BinaryCommand
	{
	public:		// [FRIENDS]
		friend BinaryInvoker;

	private:	// [DATA]
		std::streamoff m_begin;
		std::streamoff m_end;

	public:	// [LIFECYCLE]
		CLASS_CTOR		BinaryCommand(		BinaryState&			state)
			: m_begin(state.tellp())
			, m_end(-1)
		{

		}
		
		CLASS_CTOR		BinaryCommand(		BinaryCommand&&			other) noexcept
			: m_begin(other.m_begin)
			, m_end(other.m_end)
		{
			other.m_begin	= -1;
			other.m_end		= -1;
		}

		CLASS_DTOR		~BinaryCommand(){}

		BinaryCommand&	operator=(			BinaryCommand&&			other) noexcept
		{
			m_begin			= other.m_begin;
			m_end			= other.m_end;
			other.m_begin	= -1;
			other.m_end		= -1;
			return *this;
		}

	private:	// [LIFECYCLE] (deleted)
		CLASS_CTOR		BinaryCommand(		const BinaryCommand&	OTHER) = delete;
		BinaryCommand&	operator=(			const BinaryCommand&	OTHER) = delete;

	public:		// [FUNCTIONS]
		bool			was_executed() const
		{
			return m_end != -1;
		}

		void			execute(			BinaryState&			state)
		{
			state.seekg(m_begin);
			state.seekp(m_begin);
			if(m_end == -1) on_first_execution(state);
			on_execute(state);
			validate(state);
		}

		void			unexecute(			BinaryState&			state)
		{
			state.seekg(m_begin);
			state.seekp(m_begin);
			on_unexecute(state);
		}

	private:	// [INTERFACE]
		virtual void	on_first_execution(	BinaryState&			state){}
		virtual void	on_execute(			BinaryState&			state) = 0;
		virtual void	on_unexecute(		BinaryState&			state) = 0;

	private:	// [INTERNAL FUNCTIONS]
		void			validate(			BinaryState&			state)
		{
			const std::streamoff CURRENT_END = state.tellp();
			if(m_end == -1) m_end = CURRENT_END;
			if(m_end != CURRENT_END) throw GeneralException(this, __LINE__, "Each command execution must use the same space in the binary stream.");
		}
	};


	class	InvalidCommand : public GeneralException
	{
	public: 
		using GeneralException::GeneralException;
		using GeneralException::operator=;
	};


	template<typename CommandT>
	concept is_Command	=  std::is_base_of_v<BinaryCommand, CommandT>; 


	class	BinaryInvoker
	{
	private:	// [SUBTYPES]
		using MyCommands	= std::vector<std::unique_ptr<BinaryCommand>>;

	public:		// [SUBTYPES]
		static const uint32_t INVALID_INDEX = std::numeric_limits<uint32_t>::max();

	private:	// [DATA]
		BinaryState m_state;
		MyCommands	m_commands;
		uint32_t	m_currentID;

	public:		// [LIFECYCLE]
		CLASS_CTOR			BinaryInvoker()
			: m_currentID(INVALID_INDEX)
		{

		}

	public:		// [FUNCTIONS]
		template<is_Command T, typename... CTOR>
		void				invoke(			CTOR&&...			args)
		{
			if(BinaryCommand* command = BinaryInvoker::create_command<T>(std::forward<CTOR>(args)...))
			{
				command->execute(m_state);
			}
		}

		void				undo()
		{
			if(BinaryCommand* command = switch_to_previous_command())
			{
				command->unexecute(m_state);
			}
		}

		void				redo()
		{
			if(BinaryCommand* command = switch_to_next_command())
			{
				command->execute(m_state);
			}
		}

		bool				clear()
		{
			if(m_commands.empty()) return false;
			m_commands.clear();
			m_currentID = INVALID_INDEX;
			return true;
		}

	private:	// [INTERNAL FUNCTIONS]
		template<is_Command T, typename... CTOR>
		BinaryCommand*		create_command(	CTOR&&...			args)
		{
			BinaryCommand* newCommand = nullptr;
			try
			{
				trim_to_current();
				newCommand = m_commands.emplace_back(std::make_unique<T>(m_state, std::forward<CTOR>(args)...)).get();
				++m_currentID; // Note: If current is equal to INVALID_INDEX32 the value is winded back to the 0.
			}
			catch(InvalidCommand& e)
			{
				dpl::Logger::ref().push_error("[FAILED COMMAND]: %s", e.what());
			}

			return newCommand;
		}

		BinaryCommand*		switch_to_previous_command()
		{
			if(m_currentID == INVALID_INDEX) return nullptr;
			return m_commands[m_currentID--].get();
		}

		BinaryCommand*		switch_to_next_command()
		{
			if(m_currentID >= get_lastID()) return nullptr;
			return m_commands[++m_currentID].get();
		}

		uint32_t			get_lastID() const
		{
			// Note that this value may wind up to INVALID_INDEX, which is fine, since m_currentID can also have this value.
			return (uint32_t)m_commands.size() - 1;
		}

		void				trim_to_current()
		{
			if(m_currentID < get_lastID()) 
				m_commands.resize(m_currentID+1u);	
		}
	};
}


#include "dpl_StaticHolder.h"
namespace tests
{
	class GlobalCalculator : public dpl::StaticHolder<double, GlobalCalculator>
	{
	public:		// [FUNCTIONS]
		static double& value()
		{
			return data;
		}
	};

	class AddCommand : public dpl::BinaryCommand
	{
	private:	// [DATA]
		double m_value;

	public:		// [LIFECYCLE]
		CLASS_CTOR		AddCommand(		dpl::BinaryState&	state,
										const double		VALUE)
			: BinaryCommand(state)
			, m_value(VALUE)
		{

		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(		dpl::BinaryState&	state) final override
		{
			GlobalCalculator::value() += m_value;
		}

		virtual void	on_unexecute(	dpl::BinaryState&	state) final override
		{
			GlobalCalculator::value() -= m_value;
		}
	};

	class DivideByCommand : public dpl::BinaryCommand
	{
	public:		// [LIFECYCLE]
		CLASS_CTOR		DivideByCommand(dpl::BinaryState&	state,
										const double		VALUE)
			: BinaryCommand(state)
		{
			if(VALUE == 0.0) throw dpl::InvalidCommand("Can't divide by 0!");
			state.save(VALUE);
		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(		dpl::BinaryState&	state) final override
		{
			GlobalCalculator::value() /= state.load<double>();
		}

		virtual void	on_unexecute(	dpl::BinaryState&	state) final override
		{
			GlobalCalculator::value() *= state.load<double>();
		}
	};

	class MultiplyByCommand : public dpl::BinaryCommand
	{
	private:	// [DATA]
		double m_value;

	public:		// [LIFECYCLE]
		CLASS_CTOR		MultiplyByCommand(	dpl::BinaryState&	state,
											const double		VALUE)
			: BinaryCommand(state)
			, m_value(VALUE)
		{

		}

	private:	// [IMPLEMENTATION]
		virtual void	on_execute(			dpl::BinaryState&	state) final override
		{
			state.save(GlobalCalculator::value()); //<-- Save calculator value.
			GlobalCalculator::value() *= m_value;
		}

		virtual void	on_unexecute(		dpl::BinaryState&	state) final override
		{
			GlobalCalculator::value() = state.load<double>();
		}
	};

	inline void test_commands()
	{
		GlobalCalculator calc;
		double& value = calc.value();
				value = 10.0;

		dpl::BinaryInvoker invoker;

		invoker.invoke<AddCommand>(44.0);
		if(GlobalCalculator::value() != 54.0) 
			throw dpl::GeneralException(__LINE__, "Invalid value: %f", GlobalCalculator::value());

		invoker.invoke<AddCommand>(6.0);
		if(GlobalCalculator::value() != 60.0) 
			throw dpl::GeneralException(__LINE__, "Invalid value: %f", GlobalCalculator::value());

		invoker.invoke<DivideByCommand>(6.0);
		if(GlobalCalculator::value() != 10.0) 
			throw dpl::GeneralException(__LINE__, "Invalid value: %f", GlobalCalculator::value());

		invoker.invoke<MultiplyByCommand>(10.0);
		if(GlobalCalculator::value() != 100.0) 
			throw dpl::GeneralException(__LINE__, "Invalid value: %f", GlobalCalculator::value());

		invoker.undo();
		if(GlobalCalculator::value() != 10.0) 
			throw dpl::GeneralException(__LINE__, "Invalid value: %f", GlobalCalculator::value());

		invoker.redo();
		if(GlobalCalculator::value() != 100.0) 
			throw dpl::GeneralException(__LINE__, "Invalid value: %f", GlobalCalculator::value());
	}
}

/*
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
		template<is_Command T, typename... CTOR>
		Command*			update(				CTOR&&...		args)
		{
			auto newCommand = std::make_unique<T>(std::forward<CTOR>(args)...);
			if(!is_valid(newCommand.get())) return nullptr;
			trim_to_current();	
			++m_currentID; // Note: If current is equal to INVALID_INDEX32 the value is winded back to the 0.
			return m_commands.emplace_back(std::move(newCommand)).get();
		}

		inline Command*		move_backward()
		{
			if(m_currentID == INVALID_INDEX) return nullptr;
			return m_commands[m_currentID--].get();
		}

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

		inline void		undo()
		{
			if(Command* command = m_history.move_backward())
			{
				command->unexecute();
			}
		}

		inline void		redo()
		{
			if(Command* command = m_history.move_forward())
			{
				command->execute();
			}
		}

		inline bool		clear()
		{
			return m_history.clear();
		}
	};
}
*/

#pragma warning( pop )