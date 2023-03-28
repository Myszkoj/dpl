#pragma once


#include <thread>
#include <mutex>
#include <string>
#include <vector>
#include <functional>
#include <fstream>
#include <stdarg.h>
#include "dpl_ReadOnly.h"
#include "dpl_Mask.h"
#include "dpl_Indexable.h"
#include "dpl_Singleton.h"

#pragma warning( disable : 26812 ) // Unscoped enum
#pragma warning( disable : 26451 ) // Arithmetic overflow

namespace dpl
{
	/*
		Thread-safe logger.
	*/
	class Logger : public dpl::Singleton<Logger>
	{
	public: // subtypes
		enum	Category
		{
			INFO,
			WARNING,
			ERROR
		};

		class	Line : public Indexable<Line>
		{
		public: // data
			ReadOnly<Category, Line>	category;
			std::string					str;

		public: // lifecycle
			CLASS_CTOR	Line(		const Category			CATEGORY)
				: category(CATEGORY)
			{

			}

			CLASS_CTOR	Line(		const Category			CATEGORY,
									const std::string_view	STR_VIEW)
				: category(CATEGORY)
				, str(STR_VIEW)
			{

			}

			CLASS_CTOR	Line(		const Line&				OTHER) = delete;

			CLASS_CTOR	Line(		Line&&					other) noexcept = default;

		public: // operators
			Line&		operator=(	const Line&				OTHER) = delete;

			Line&		operator=(	Line&&					other) noexcept = default;
		};

		using	Lines = std::vector<Line>;

		class	Filter	: public dpl::Mask32<Category>
						, public Indexer<Line>
		{
		public: // functions
			inline Line&	try_line(	Line&	line)
			{
				if(at(line.category())) add(line);
				return line;
			}

			inline void		update(		Lines&	lines)
			{
				remove_all();
				for(auto& iLine : lines) if(at(iLine.category())) add(iLine);
			}
		};

	public: // constants
		static const uint32_t	NUM_CATEGORIES	= 3;
		static const size_t		MAX_MSG_SIZE	= 512;

	public: // data
		ReadOnly<Lines,		Logger> lines;	// Accessing lines is not thread safe!
		ReadOnly<uint32_t,	Logger> counters[NUM_CATEGORIES]; // One for each category. Accessing counters is not thread safe! (use dedicated function)
		ReadOnly<Filter,	Logger> filter; // Accessing filter is not thread safe!

	private: // data
		mutable std::mutex m_mtx;

	public: // lifecycle
		CLASS_CTOR				Logger(				Multiton&							multition)
			: Singleton(multition)
		{

		}

	public: // thread safe functions
		/*
			Returns total number of lines.
		*/
		inline uint32_t			get_numLines() const
		{
			std::lock_guard lock(m_mtx);
			return static_cast<uint32_t>(lines().size());
		}

		/*
			Returns number of lines of the given category.
		*/
		inline uint32_t			get_numLines(		const Category						CATEGORY) const
		{
			std::lock_guard lock(m_mtx);
			return counters[CATEGORY];
		}

		inline void				show_all_categories()
		{				
			std::lock_guard lock(m_mtx);
			filter->set_at(INFO,	true);
			filter->set_at(WARNING,	true);
			filter->set_at(ERROR,	true);
			filter->update(*lines);
		}

		inline void				show_category(		const Category						CATEGORY)
		{
			std::lock_guard lock(m_mtx);
			filter->set_at(CATEGORY, true);
			filter->update(*lines);
		}

		inline void				toggle_category(	const Category						CATEGORY)
		{
			std::lock_guard lock(m_mtx);
			filter->toggle_at(CATEGORY);
			filter->update(*lines);
		}

		inline void				hide_category(		const Category						CATEGORY)
		{
			std::lock_guard lock(m_mtx);
			filter->set_at(CATEGORY, false);
			filter->update(*lines);
		}

		/* UNSAFE
		inline std::string&		emplace_message(	const Category								CATEGORY)
		{
			std::lock_guard lock(m_mtx);
			++(*counters[CATEGORY]);
			return filter->try_line(lines->emplace_back(CATEGORY)).str;
		}

		inline std::string&		emplace_info()
		{
			return emplace_message(INFO);
		}

		inline std::string&		emplace_warning()
		{
			return emplace_message(WARNING);
		}

		inline std::string&		emplace_error()
		{
			return emplace_message(ERROR);
		}

		inline void				emplace_message(	const Category								CATEGORY,
													const std::function<void(std::string&)>&	FUNCTION)
		{
			std::lock_guard lock(m_mtx);
			++(*counters[CATEGORY]);
			FUNCTION(filter->try_line(lines->emplace_back(CATEGORY)).str);
		}

		*/

		std::uintptr_t			push_message(		const Category						CATEGORY,
													const std::string_view				MESSAGE)
		{
			std::lock_guard lock(m_mtx);
			++(*counters[CATEGORY]);
			filter->try_line(lines->emplace_back(CATEGORY, MESSAGE));
			return 0;
		}

		std::uintptr_t			push_message(		const Category						CATEGORY, 
													const char*							FORMAT, ...)
		{
			std::lock_guard lock(m_mtx);
			++(*counters[CATEGORY]);
			auto& message = filter->try_line(lines->emplace_back(CATEGORY)).str;
			message.resize(MAX_MSG_SIZE);
			va_list ap;
			va_start(ap, FORMAT);
			const int LENGTH = vsnprintf(message.data(), MAX_MSG_SIZE, FORMAT, ap);
			va_end(ap);
			message.resize(LENGTH);
			message.shrink_to_fit();
			return 0;
		}

		inline std::uintptr_t	push_info(			const std::string_view				MESSAGE)
		{
			return push_message(INFO, MESSAGE);
		}

		inline std::uintptr_t	push_warning(		const std::string_view				MESSAGE)
		{
			return push_message(WARNING, MESSAGE);
		}

		inline std::uintptr_t	push_error(			const std::string_view				MESSAGE)
		{
			return push_message(ERROR, MESSAGE);
		}

		template<typename... Args>
		inline std::uintptr_t	push_info(			const char*							FORMAT,
													Args...								args)
		{
			return Logger::push_message(INFO, FORMAT, args...);
		}

		template<typename... Args>
		inline std::uintptr_t	push_warning(		const char*							FORMAT,
													Args...								args)
		{
			return Logger::push_message(WARNING, FORMAT, args...);
		}

		template<typename... Args>
		inline std::uintptr_t	push_error(			const char*							FORMAT,
													Args...								args)
		{
			return Logger::push_message(ERROR, FORMAT, args...);
		}

		inline void				throw_runtime_error() const
		{
			throw std::runtime_error("Errors detected: " + get_numLines(ERROR));
		}

		inline void				clear_category(		const Category						CATEGORY)
		{
			std::lock_guard lock(m_mtx);
			if(counters[CATEGORY]() == 0) return;
			Lines newLines; newLines.reserve(counters[(CATEGORY+1)%NUM_CATEGORIES]() + counters[(CATEGORY+2)%NUM_CATEGORIES]());
			move_lines(CATEGORY, newLines);
			if(filter().any() && filter().at(CATEGORY)) filter->update(*lines);
			counters[CATEGORY]	= 0;
		}

		inline void				clear()
		{
			std::lock_guard lock(m_mtx);
			lines				= std::move(Lines());
			counters[INFO]		= 0;
			counters[WARNING]	= 0;
			counters[ERROR]		= 0;
		}

		inline bool				export_lines(		const std::string&					FILE) const
		{
			std::ofstream fout(FILE, std::ios::trunc);
			if(fout.fail() || fout.bad()) return false;
			for(auto& iLine : lines()) fout << iLine.str + "\n";
			fout.close();
			return true;
		}

		inline void				log_if_exception(	const std::function<void()>&		FUNCTION,
													const char*							FILE,
													const int							LINE)
		{
			if (no_except(FUNCTION)) return;
			push_error("Silent exception thrown in %s at line: %d", FILE, LINE);
		}

	private: // functions
		inline void				move_lines(			const Category						INVALID_CATEGORY,
													Lines&								destination)
		{
			const uint32_t NUM_LINES = static_cast<uint32_t>(lines().size());
			for(uint32_t index = 0; index < NUM_LINES; ++index)
			{
				auto& line = (*lines)[index];
				if(line.category != INVALID_CATEGORY)
				{
					destination.emplace_back(std::move(line));
				}
			}
			lines = std::move(destination);
		}
	};
}