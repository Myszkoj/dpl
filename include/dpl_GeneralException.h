#pragma once


#include <stdexcept>
#include <typeinfo>
#include <string>
#include <functional>
#include <stdarg.h>
#include "dpl_ClassInfo.h"


namespace dpl
{
	/*
		Supports additional information about object that throws exception like line or class type.
	*/
	class GeneralException : public std::runtime_error
	{
	public: // subtypes
		using Base = std::runtime_error;

	public: // constants
		static const size_t	MAX_MSG_SIZE = 512;

	public: // basic versions
		CLASS_CTOR							GeneralException(	const std::string&		MESSAGE)
			: Base(MESSAGE + end_line())
		{
		}

		CLASS_CTOR							GeneralException(	const int				LINE, 
																const std::string&		MESSAGE)
			: Base(line_message(LINE) + error_message(MESSAGE) + end_line())
		{

		}

		CLASS_CTOR							GeneralException(	const char*				FILE,		
																const int				LINE, 
																const std::string&		message)
			: Base(file_message(FILE) + line_message(LINE) + error_message(message) + end_line())
		{

		}

		CLASS_CTOR							GeneralException(	const char*				FILE,		
																const int				LINE, 
																const char*				FORMAT, ...)
			: Base(file_message(FILE) + line_message(LINE) + format_str(FORMAT) + end_line())
		{

		}

		template<class T>
		CLASS_CTOR							GeneralException(	const T*				CLASS_TYPE_DUMMY, 
																const std::string&		MESSAGE)
			: Base(class_message<T>() + error_message(MESSAGE) + end_line())
		{

		}

		template<class T>
		CLASS_CTOR							GeneralException(	const T*				CLASS_TYPE_DUMMY, 
																const int				LINE, 
																const std::string&		MESSAGE)
			: Base(class_message<T>() + line_message(LINE) + error_message(MESSAGE) + end_line())
		{

		}

		template<class T>
		CLASS_CTOR							GeneralException(	const T*				CLASS_TYPE_DUMMY,		
																const int				LINE, 
																const char*				FORMAT, ...)
			: Base(class_message<T>() + line_message(LINE) + format_str(FORMAT) + end_line())
		{

		}

	public: // prefixed versions
		CLASS_CTOR							GeneralException(	const GeneralException& PREV,
																const std::string&		MESSAGE)
			: Base(PREV.what() + MESSAGE + end_line())
		{
		}

		CLASS_CTOR							GeneralException(	const GeneralException& PREV,
																const int				LINE, 
																const std::string&		MESSAGE)
			: Base(PREV.what() + line_message(LINE) + error_message(MESSAGE) + end_line())
		{

		}

		CLASS_CTOR							GeneralException(	const GeneralException& PREV,
																const char*				FILE,		
																const int				LINE, 
																const std::string&		message)
			: Base(PREV.what() + file_message(FILE) + line_message(LINE) + error_message(message) + end_line())
		{

		}

		template<class T>
		CLASS_CTOR							GeneralException(	const GeneralException& PREV,
																const T*				CLASS_TYPE_DUMMY, 
																const std::string&		MESSAGE)
			: Base(PREV.what() + class_message<T>() + error_message(MESSAGE) + end_line())
		{

		}

		template<class T>
		CLASS_CTOR							GeneralException(	const GeneralException& PREV,
																const T*				CLASS_TYPE_DUMMY, 
																const int				LINE, 
																const std::string&		MESSAGE)
			: Base(PREV.what() + class_message<T>() + line_message(LINE) + error_message(MESSAGE) + end_line())
		{

		}

	public: // postfixed versions
		CLASS_CTOR							GeneralException(	const std::string&		MESSAGE,
																const GeneralException& NEXT)
			: Base(MESSAGE + end_line() + NEXT.what())
		{

		}

		CLASS_CTOR							GeneralException(	const int				LINE, 
																const std::string&		MESSAGE,
																const GeneralException& NEXT)
			: Base(line_message(LINE) + error_message(MESSAGE) + end_line() + NEXT.what())
		{

		}

		CLASS_CTOR							GeneralException(	const char*				FILE,		
																const int				LINE, 
																const std::string&		message,
																const GeneralException& NEXT)
			: Base(file_message(FILE) + line_message(LINE) + error_message(message) + end_line() + NEXT.what())
		{

		}

		template<class T>
		CLASS_CTOR							GeneralException(	const T*				CLASS_TYPE_DUMMY, 
																const std::string&		MESSAGE,
																const GeneralException& NEXT)
			: Base(class_message<T>() + error_message(MESSAGE) + end_line() + NEXT.what())
		{

		}

		template<class T>
		CLASS_CTOR							GeneralException(	const T*				CLASS_TYPE_DUMMY, 
																const int				LINE, 
																const std::string&		MESSAGE,
																const GeneralException& NEXT)
			: Base(class_message<T>() + line_message(LINE) + error_message(MESSAGE) + end_line() + NEXT.what())
		{

		}

	protected: // functions
		template<class T>
		inline std::string					class_message()
		{
			return std::string("Exception in: [") + typeid(T).name() + std::string("] ");
		}

		inline std::string					file_message(		const char*				FILE) const
		{
			return "file: [" + std::string(FILE) + "] ";
		}

		inline std::string					line_message(		const int				LINE) const
		{
			return "line: [" + std::to_string(LINE) + "] ";
		}

		inline std::string					error_message(		const std::string&		MESSAGE) const
		{
			return " error message: " + MESSAGE;
		}

		static std::string					format_str(			const char*				FORMAT, ...)
		{
			std::string message;
			message.resize(MAX_MSG_SIZE);
			va_list ap;
			va_start(ap, FORMAT);
			const int LENGTH = vsnprintf(message.data(), MAX_MSG_SIZE, FORMAT, ap);
			va_end(ap);
			message.resize(LENGTH);
			return message;
		}

		static inline const std::string&	end_line()
		{
			static const std::string END_LINE = "\n";
			return END_LINE;
		}
	};

	/*
		Catches all exceptions thrown by the function.
		Returns false if at least one exception was caught.
	*/
	inline bool		no_except(			const std::function<void()>&	FUNCTION) noexcept
	{
		try{FUNCTION();}
		catch(...){return false;}
		return true;
	}
}