#pragma once


#include <utility>
#include "dpl_ClassInfo.h"


#pragma pack(push, 1)

namespace dpl
{
	/*
		Wrapps DataT so that all WRITE operations are accessible only to OwnerT.
	*/
	template<typename DataT, typename OwnerT>
	class ReadOnly
	{
	private: // data
		DataT data;

	public: // lifecycle
		template<typename... Args>
		CLASS_CTOR			ReadOnly(	Args&&...					args) 
			: data(std::forward<Args>(args)...)
		{
		}

		CLASS_CTOR			ReadOnly(	ReadOnly&&					other) noexcept
			: data(std::move(other.data))
		{
		}

		CLASS_CTOR			ReadOnly(	const ReadOnly&				other)
			: data(other.data)
		{
		}

	public: // access
		inline const DataT&	operator()() const
		{
			return data;
		}

		inline operator		const DataT&() const
		{
			return data;
		}

		inline bool			operator==(	const DataT&				other) const
		{
			return data == other;
		}

		inline bool			operator!=(	const DataT&				other) const
		{
			return data != other;
		}

		inline bool			operator==(	const ReadOnly&				other) const
		{
			return data == other.data;
		}

		inline bool			operator!=(	const ReadOnly&				other) const
		{
			return data != other.data;
		}

		inline bool			operator&&(	const DataT&				other) const
		{
			return data && other;
		}

		inline bool			operator||(	const DataT&				other) const
		{
			return data || other;
		}

	private: friend OwnerT; // access
		inline void			swap(		ReadOnly<DataT, OwnerT>&	other)
		{
			std::swap(data, other.data);
		}

		inline DataT&		operator*()
		{
			return data;
		}

		inline DataT*		operator->()
		{
			return &data;
		}

		inline ReadOnly&	operator=(	ReadOnly&&					other) noexcept
		{
			data = std::move(other.data);
			return *this;
		}

		inline ReadOnly&	operator=(	const ReadOnly&				other)
		{
			if(this != &other)
			{
				data = other.data;
			}

			return *this;
		}

		inline ReadOnly&	operator=(	const DataT&				data)
		{
			this->data = data;
			return *this;
		}

		inline ReadOnly&	operator=( DataT&&						data) noexcept
		{
			this->data = std::move(data);
			return *this;
		}
	};


	/*
		Wrapps DataT array so that all WRITE operations are accessible only to OwnerT.
	*/
	template<typename DataT, unsigned int N, typename OwnerT>
	class ReadOnly<DataT[N], OwnerT>
	{
	private: // data
		DataT data[N];

	public: // lifecycle
		template<typename... Args>
		CLASS_CTOR			ReadOnly(	Args&&...			args) 
			: data{DataT(std::forward<Args>(args)...)}
		{
		}

		CLASS_CTOR			ReadOnly(	DataT d) 
			: data{d}
		{
		}

		CLASS_CTOR			ReadOnly(	const ReadOnly&		other)
		{
			for(unsigned int i = 0; i < N; ++i)
			{
				data[i] = other.data[i];
			}
		}

	public: // access
		const DataT&		operator()(	const uint32_t		INDEX) const
		{
			return data[INDEX];
		}

		bool				operator==(	const DataT&		other) const
		{
			for(unsigned int i = 0; i < N; ++i)
			{
				if(data[i] != other)
					return false;
			}

			return true;
		}

		bool				operator!=(	const DataT&		other) const
		{
			for(unsigned int i = 0; i < N; ++i)
			{
				if(data[i] != other)
					return true;
			}

			return false;
		}

		bool				operator==(	const ReadOnly&		other) const
		{
			for(unsigned int i = 0; i < N; ++i)
			{
				if(data[i] != other.data[i])
					return false;
			}

			return true;
		}

		bool				operator!=(	const ReadOnly&		other) const
		{
			for(unsigned int i = 0; i < N; ++i)
			{
				if(data[i] != other.data[i])
					return true;
			}

			return false;
		}

	private: friend OwnerT; // access
		DataT&				operator[](	unsigned int		i)
		{
			return data[i];
		}

		const DataT&		operator[](	unsigned int		i) const
		{
			return data[i];
		}

		ReadOnly&			operator=(	ReadOnly&&			other)
		{
			if(this != &other)
			{
				for(unsigned int i = 0; i < N; ++i)
				{
					data[i] = std::move(other.data[i]);
				}
			}

			return *this;
		}

		ReadOnly&			operator=(	const ReadOnly&		other)
		{
			if(this != &other)
			{
				for(unsigned int i = 0; i < N; ++i)
				{
					data[i] = other.data[i];
				}
			}

			return *this;
		}

		ReadOnly&			operator=(	const DataT&		data)
		{
			for(unsigned int i = 0; i < N; ++i)
			{
				this->data[i] = data;
			}

			return *this;
		}
	};
}

#pragma pack(pop)