#include "..//include/complex_Application.h"


// Exit
namespace complex
{
	CLASS_CTOR	Exit::Exit(	const Binding&	BINDING)
		: ProgramState(BINDING)
		, response(Response::WAITING)
	{
			
	}

	void		Exit::begin(	Progress&	progress)
	{
		progress.reset(0, "Exiting...");
		response = Response::WAITING;
	}

	void		Exit::update(	Progress&	progress)
	{
		if(condition())
		{
			response = condition()();
			
			switch(response())
			{
			case Response::ABORTED:		return set_previous_state();
			case Response::CONFIRMED:	return set_next_state<NullState>();
			default:					return; // Waiting for response...
			}
		}
		else // No condition required,just exit.
		{
			response = Response::CONFIRMED;
			set_next_state<NullState>();
		}
	}

	void		Exit::end()
	{
		if(response != Response::CONFIRMED) return;
		Application::ref().request_shutdown();
	}
}

// Application
namespace complex
{
	CLASS_CTOR	Application::Application(				const std::string&				NAME,
														const int						ARG_COUNT, 
														char**const						ARG_VECTOR,
														const uint32_t					NUM_THREADS)
		: SystemManager(NAME + SETTINGS_EXT, NUM_THREADS)
		, name(NAME)
		, argc(ARG_COUNT)
		, argv(ARG_VECTOR)
	{
		if(SDL_Init(SDL_INIT_VIDEO) < 0)
			throw dpl::GeneralException(this, __LINE__, "Fail to initialize SDL.");

		add_state<Exit>();
	}

	CLASS_DTOR	Application::~Application()
	{
		safe_release();
		SDL_Quit();
	}

	void		Application::handle_installation(		const std::function<void()>&	INSTALL_ALL_SYSTEMS)
	{
		try
		{
			TimeManager::reset();	
			flags->set_at(WORKING, true);
			initialize_GUI();
			INSTALL_ALL_SYSTEMS();
			flags->set_at(STARTED, true);
		}
		catch(const dpl::GeneralException& e)
		{
			auto&	message = get_logger().emplace_error();
					message = "Application >> ";
					message += e.what();

			terminate();
			return;
		}
		catch(...)
		{
			get_logger().push_error("Application >> Unknown error");			
			terminate();
			return;
		}
	}

	bool		Application::set_next_cycle()
	{
		if(!flags().at(WORKING)) return false;
		if(flags().at(SHUTDOWN)) return false;
		TimeManager::update();
		return true;
	}

	bool		Application::main_loop()
	{
		while(set_next_cycle())
		{
			try
			{
				update_states(get_logger());
				update_all_systems();
				update_events();
				mainWindow()->update(*this);
			}
			catch(const dpl::GeneralException& e)
			{
				auto&	message = get_logger().emplace_error();
						message = "Fail to update: ";
						message += e.what();

				return false;
			}
			catch(...)
			{
				get_logger().push_error("Fail to update: UNKNOWN_ERROR.");
				return false;
			}
		}

		return true;
	}

	void		Application::shutdown()
	{
		if(flags().at(WORKING))
		{		
			try
			{
				release_states(get_logger());
				uninstall_all_systems();
			}
			catch(const dpl::GeneralException& e)
			{
				auto&	message = get_logger().emplace_error();
						message = "Fail to shutdown application: ";
						message += e.what();

				terminate();
				return;
			}
			catch(...)
			{
				get_logger().push_error("Fail to shutdown application: UNKNOWN_ERROR");
				terminate();
				return;
			}

			flags->set_at(WORKING, false);
			safe_release();
			get_logger().export_lines("log.txt");
		}

		flags->clear();
	}

	void		Application::terminate()
	{
		show_error("Crash", "Unexpected program termination. See log.txt for more info."); //<-- This can be invoked through event.
		get_logger().export_lines("log.txt");
		flags->clear();
	}

	void		Application::safe_release() noexcept
	{
		if(!flags().at(INSTALLED)) return;
		dpl::no_except([&](){ImGui_ImplOpenGL3_Shutdown();});
		dpl::no_except([&](){ImGui_ImplSDL2_Shutdown();});
		dpl::no_except([&](){ImPlot::DestroyContext();});
		dpl::no_except([&](){ImGui::DestroyContext();});
		dpl::no_except([&](){mainWindow->reset();});
		flags->set_at(INSTALLED, false);
	}

	// OBSOLETE
	/*
	void		Application::request(					const Request					REQUEST)
	{
		switch(REQUEST)
		{
		case Request::SAVE_PROJECT:
			if(project.name().empty()) requests->set_at(SAVE_PROJECT_AS, true);
			break;

		case Request::OPEN_PROJECT:
		case Request::NEW_PROJECT:
			requests->set_at(CLOSE_PROJECT, true);
			if(requests().at(EXIT)) return; // ignore request if we are about to exit the program
			break;

		case Request::EXIT:
			requests->set_at(CLOSE_PROJECT, true);
			break;

		default: return;
		}

		requests->set_at(REQUEST, true);
	}
	*/
	/*
	void		Application::handle_SAVE_AS_request()
	{
		if(project.flags().at(Project::UNSAVED))
		{
			if(!newProjectPath.has_value())
			{
				newProjectPath = mainWindow()->saveFileDialog(	get_windows_filter("Project file (.#) |*.#|", Project::ext()).c_str(), 
																get_windows_filter("#|", Project::ext()).c_str(), true);
			}
			
			if(newProjectPath.has_value())
			{
				if(newProjectPath != project.get_fullPath())
				{
					switch(confirm_SAVE_AS())
					{
					case Status::CONFIRM:
						project.set_path(newProjectPath.value(), get_logger(), [&](const std::string& FILE_PATH)
						{
							std::ofstream file(FILE_PATH, std::ios::binary | std::ios::trunc);
							if(file.fail() || file.bad())
							{
								get_logger().push_error("Could not save project with given path: " + FILE_PATH);
								return false;
							}
							else
							{
								on_save_project();
								project.set_saved();
								file.close();
								return true;
							}
						});	
						break;

					case Status::WAIT:
						return; // ... until CONFIRM or DISCARD

					default:
						break;
					}
				}
				
				// User used the same file path, request standard SAVE method.
				request(SAVE_PROJECT);
			}
		}

		newProjectPath.reset();
		requests->set_at(SAVE_PROJECT_AS, false);
	}

	void		Application::handle_SAVE_request()
	{
		if(project.flags().at(Project::UNSAVED))
		{
			switch(confirm_SAVE())
			{
			case Status::CONFIRM:
				{
					const auto FILE_PATH = project.get_fullPath();
					std::ofstream file(FILE_PATH, std::ios::binary | std::ios::trunc);
					if(file.fail() || file.bad())
					{
						get_logger().push_error("Could not save project with given path: " + FILE_PATH);
					}
					else
					{
						on_save_project();
						project.set_saved();
						file.close();
					}
				}
				break;

			case Status::WAIT:
				return; // ... until CONFIRM or DISCARD

			default:
				break;
			}
		}

		requests->set_at(SAVE_PROJECT, false);
	}

	void		Application::handle_CLOSE_request()
	{
		if(project.flags().at(Project::OPEN))
		{
			switch(confirm_CLOSE())
			{
			case Status::CONFIRM:
				project.close();
				break;

			case Status::WAIT:
				return; // ... until CONFIRM or DISCARD

			default: // ignore requests that may have caused this request:
				requests->set_at(OPEN_PROJECT, false);
				requests->set_at(NEW_PROJECT, false);
				requests->set_at(EXIT, false); //<-- Can't exit without closing the project.
				break;
			}
		}

		requests->set_at(CLOSE_PROJECT, false);
	}

	void		Application::handle_EXIT_request()
	{
		switch(confirm_EXIT())
		{
		case Status::CONFIRM:
			flags->set_at(SHUTDOWN, true);
			requests->clear(); // ignore other requests
			break;

		case Status::WAIT:
			return; // ... until CONFIRM or DISCARD

		default:
			break;
		}

		newProjectPath.reset();
		requests->set_at(EXIT, false);
	}

	void		Application::handle_OPEN_request()
	{
		if(!newProjectPath.has_value())
		{
			newProjectPath = mainWindow()->openFileDialog(	get_windows_filter("Project file (.#) |*.#|", Project::ext()).c_str(), 
															get_windows_filter("#|", Project::ext()).c_str());
		}

		if(newProjectPath.has_value())
		{
			switch(confirm_OPEN(newProjectPath == project.get_fullPath()))
			{
			case Status::CONFIRM:
				project.set_path(newProjectPath.value(), get_logger(), [&](const std::string& FILE_PATH)
				{
					std::ifstream file(FILE_PATH, std::ios::binary);
					if(file.fail() || file.bad())
					{
						get_logger().push_error("Could not open given project file: " + FILE_PATH);
						return false;
					}
					else
					{
						project.close();
						on_load_project();
						file.close();
						return true;
					}
				});
				break;

			case Status::WAIT:
				return; // ... until CONFIRM or DISCARD

			default:
				break;
			}
		}

		newProjectPath.reset();
		requests->set_at(OPEN_PROJECT, false);
	}

	void		Application::handle_NEW_request()
	{
		newProjectPath.reset();
		on_new_project();
		project.set_unsaved();
		requests->set_at(NEW_PROJECT, false);
	}

	void		Application::handle_all_requests()
	{
		if(requests().any())
		{
			if(requests().at(SAVE_PROJECT_AS))		handle_SAVE_AS_request();
			else if(requests().at(SAVE_PROJECT))	handle_SAVE_request();		
			else if(requests().at(CLOSE_PROJECT))	handle_CLOSE_request();
			else if(requests().at(EXIT))			handle_EXIT_request();
			else if(requests().at(OPEN_PROJECT))	handle_OPEN_request();
			else if(requests().at(NEW_PROJECT))		handle_NEW_request();
		}
	}
	*/
}

// TODO: Move to the GUI system.
namespace complex
{
	void		Application::show_error(				const char*						TITLE,
														const char*						MESSAGE) const
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, TITLE, MESSAGE, nullptr);
	}

	void		Application::show_warning(				const char*						TITLE,
														const char*						MESSAGE) const
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, TITLE, MESSAGE, nullptr);
	}

	void		Application::show_information(			const char*						TITLE,
														const char*						MESSAGE) const
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, TITLE, MESSAGE, nullptr);
	}

	void		Application::initialize_GUI()
	{
		*mainWindow = std::make_unique<MainWindow>(name());

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImPlot::CreateContext();

		ImGuiIO&	io = ImGui::GetIO();// (void)io;
					io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
					io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
					io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		//io.ConfigViewportsNoAutoMerge = true;
		//io.ConfigViewportsNoTaskBarIcon = true;

		ImGui::StyleColorsDark();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		// Setup Platform/Renderer bindings
		ImGui_ImplSDL2_InitForOpenGL(mainWindow()->handle, mainWindow()->context.get());
		ImGui_ImplOpenGL3_Init();
	}

	void		Application::update_events()
	{
		MainWindow::get_relative_mouse_state()	? ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse
												: ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
		
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			switch(event.type)
			{
			case SDL_APP_TERMINATING:
				return; // Ignore all other events and exit.

			case SDL_QUIT:
				set_next_state<Exit>();
				break;

			case SDL_WINDOWEVENT:
				if(event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == mainWindow()->get_internalID())
				{ 
					set_next_state<Exit>();
				}
				break;

			default:
				break;
			}

			if(mainWindow()) mainWindow()->on_event(event);
		}
	}
}