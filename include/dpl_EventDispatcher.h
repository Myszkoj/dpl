#pragma once


#include "dpl_Singleton.h"
#include "dpl_NamedType.h"
#include "dpl_Indexable.h"
#include "dpl_Variation.h"
#include "dpl_Subject.h"
#include <mutex>


// forward declarations
namespace dpl
{
	using Event = IType;

	class EventDispatcher;
	class Device;
	class Emitter;

	template<typename EventT>
	class Transmitter;

	template<typename EventT>
	class Receiver;
}

// implementations
namespace dpl
{
	template<typename EventT>
	class EventType : public NamedType<EventT>
					, public IndexableType<EventT>
	{

	};

	template<typename T>
	inline void validate_event_type()
	{
		static_assert(std::is_base_of<EventType<T>, T>::value, "All events must be derived from the EventType<T>, where T is the type of the event.");
	}

	/*
		Base class for Transmitter types.
	*/
	class Device : public Variant<EventDispatcher, Device>
	{
	public: // functions
		CLASS_CTOR			Device(		const Binding&		BINDING)
			: Variant(BINDING)
		{

		}

		CLASS_DTOR virtual	~Device(){}
	};


	/*
		Broadcasts events to all listening receivers.

		Warning! You have to disable transmitter before destructor is called.
	*/
	template<typename EventT>	
	class Transmitter	: public Device
						, public Subject<Transmitter<EventT>>
	{
	private: // subtypes
		using MyReceivers = Subject<Transmitter<EventT>>;

	public: // relations
		friend Receiver<EventT>;

	private: // data
		const EventT*	EVENT;

	public: // lifecycle
		CLASS_CTOR	Transmitter(const Binding&	BINDING)
			: Device(BINDING)
			, EVENT(nullptr)
		{
			dpl::validate_event_type<EventT>();
		}

	public: // functions
		inline void update(		const EventT&	NEW_EVENT)
		{
			this->EVENT	= &NEW_EVENT;
			MyReceivers::notify_observers();
		}
	};


	/*
		Broadcasts events.
	*/
	class EventDispatcher	: public Singleton<EventDispatcher>
							, private Variation<EventDispatcher, Device>
							, private Group<EventDispatcher, Emitter>
	{
	public: // subtypes
		using MyTransmitters	= Variation<EventDispatcher, Device>;
		using MyEmitters		= Group<EventDispatcher, Emitter>;

	public: // relations
		friend Variant<EventDispatcher, Device>;
		friend MyTransmitters;
		friend MyEmitters;
		friend Emitter;

	private: // data
		std::mutex m_mtx;

	public: // functions
		template<typename EventT>
		/*
			Sends event to all listening receivers.
		*/
		inline void					broadcast(	const EventT&	EVENT)
		{
			if(Transmitter<EventT>* transmitter = find_transmitter<EventT>())
			{
				transmitter->update(EVENT);
			}
		}

	private: template<typename> friend class Receiver;
		template<typename EventT>
		inline Transmitter<EventT>*	find_transmitter()
		{
			std::lock_guard lock(m_mtx);
			return MyTransmitters::find_variant<Transmitter<EventT>>();
		}

		template<typename EventT>
		inline Transmitter<EventT>*	get_transmitter()
		{
			std::lock_guard lock(m_mtx);
			auto result = MyTransmitters::create_variant<Transmitter<EventT>>();
			return result.get();
		}
	};


	/*
		Emits any events.
	*/
	class Emitter : private Member<EventDispatcher, Emitter>
	{
	private: // subtypes
		using MyBase = Member<EventDispatcher, Emitter>;

	public: // relations
		friend MyBase;
		friend EventDispatcher;
		friend Sequence<Emitter>;
		friend Sequenceable<Emitter>;
		friend Group<EventDispatcher, Emitter>;

	public: // lifecycle
		CLASS_CTOR			Emitter(	EventDispatcher*	dispatcher = nullptr)
		{
			setup(dispatcher);
		}

	public: // functions
		template<typename EventT>
		inline void			emit(		const EventT&		EVENT)
		{
			if(EventDispatcher* dispatcher = get_chain())
			{
				dispatcher->broadcast(EVENT);
			}
		}

	protected: // functions
		inline void			setup(		EventDispatcher*	newDispatcher)
		{
			if(newDispatcher)
			{
				newDispatcher->add_end_member(*this);
			}
			else
			{
				Member::detach();
			}
		}
	};


	template<typename EventT>
	class Receiver : private Observer<Transmitter<EventT>>
	{
	public: // subtypes
		using MyBase = Observer<Transmitter<EventT>>;

	public: // relations
		friend MyBase;
		friend Transmitter<EventT>;

	public: // lifecycle
		CLASS_CTOR		Receiver()
		{
			dpl::validate_event_type<EventT>();
		}

		CLASS_CTOR		Receiver(	EventDispatcher&		dispatcher)
			: Receiver()
		{
			listen(dispatcher);
		}

	protected: // functions
		inline void		listen(		Transmitter<EventT>&	transmitter)
		{
			MyBase::observe(transmitter);
		}

		inline void		listen(		EventDispatcher&		dispatcher)
		{
			MyBase::observe(*dispatcher.get_transmitter<EventT>());
		}

		inline void		disable()
		{
			MyBase::stop_observation();
		}

	private: // functions
		virtual void	on_event(	const EventT&			EVENT) = 0;

	private: // implementation
		virtual void	on_update(	Transmitter<EventT>&	transmitter) final override
		{
			this->on_event(*transmitter.EVENT);
		}
	};
}