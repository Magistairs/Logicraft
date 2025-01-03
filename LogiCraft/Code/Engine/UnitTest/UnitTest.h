
#include "Engine/Core/EventSystem.h"
#include "Engine/Core/TaskManager.h"
#include "Objects/GameObjectManager.h"

#include <SFML/System/Clock.hpp>
#include <assert.h>
#include <cstdlib>
#include <ctime>
#include <shared_mutex>
#include <vector>

namespace Logicraft
{
class ObjectsManagerTests
{
public:
	ObjectsManagerTests(sf::Int32 timeOut)
	  : timeOut(timeOut)
	{
	}

	void Run()
	{
		Logger::Get().Log(Logger::eInfo, "Begin ObjectsManager test");

		// ActionManager::Get().ExecuteAction("GameObjectManager_enable_log");

		const char* cpntType = GameComponent::GetRegisteredTypes()[0]->GetName().c_str();

		// Create objects
		++m_runningTasks;
		TaskManager::Get().AddTask([this]() {
			for (int i = 0; i < s_numOfWorkingThreadsByAction; i++)
			{
				++m_runningTasks;
				TaskManager::Get().AddTask([this]() {
					for (int j = 0; j < s_numObjectsToAddByThread; j++)
					{
						GameObjectPtr pObject = GameObjectManager::Get().CreateObject();

						std::lock_guard<std::shared_mutex> lock(m_objectsMutex);
						m_objects.push_back(pObject);
						++m_objectsAdded;
					}
					--m_runningTasks;
				});
			}
			--m_runningTasks;
		});

		// Create components
		++m_runningTasks;
		TaskManager::Get().AddTask([this, cpntType]() {
			for (int i = 0; i < s_numOfWorkingThreadsByAction; i++)
			{
				++m_runningTasks;
				TaskManager::Get().AddTask([this, cpntType]() {
					for (int j = 0; j < s_numComponentsToAddByThread; j++)
					{
						GameComponentPtr pCpnt = GameObjectManager::Get().CreateComponent(cpntType);
						if (GameObjectPtr pObject = GetRandomObject(false))
						{
							pObject->AddComponent(pCpnt);
						}
						std::lock_guard<std::shared_mutex> lock(m_componentsMutex);
						m_components.push_back(pCpnt);
						++m_componentsAdded;
					}
					--m_runningTasks;
				});
			}
			--m_runningTasks;
		});

		// Destroy components
		++m_runningTasks;
		TaskManager::Get().AddTask([this]() {
			for (int i = 0; i < s_numOfWorkingThreadsByAction; i++)
			{
				++m_runningTasks;
				TaskManager::Get().AddTask([this]() {
					while ((m_componentsAdded < s_numComponentsToAdd || m_componentsAdded > m_componentsRemoved) && !m_stop)
					{
						if (GameComponentPtr pCpnt = GetRandomComponent())
						{
							GameObjectManager::Get().RemoveComponent(pCpnt->GetGUID());
						}
					}
					--m_runningTasks;
				});
			}
			--m_runningTasks;
		});

		// Destroy objects
		++m_runningTasks;
		TaskManager::Get().AddTask([this]() {
			for (int i = 0; i < s_numOfWorkingThreadsByAction; i++)
			{
				++m_runningTasks;
				TaskManager::Get().AddTask([this]() {
					while ((m_objectsAdded < s_numObjectsToAdd || m_objectsAdded > m_objectsRemoved) && !m_stop)
					{
						if (GameObjectPtr pObject = GetRandomObject(true))
						{
							GameObjectManager::Get().RemoveObject(pObject->GetGUID());
						}
					}
					--m_runningTasks;
				});
			}
			--m_runningTasks;
		});

		Stop();
		Logger::Get().Log(Logger::eInfo, "End ObjectsManager test");
	}

	GameObjectPtr GetRandomObject(bool remove)
	{
		std::lock_guard<std::shared_mutex> lock(m_objectsMutex);
		if (!m_objects.empty())
		{
			int           objectToRemove = std::rand() % m_objects.size();
			GameObjectPtr pObject        = m_objects[objectToRemove];
			if (remove)
			{
				m_objects.erase(m_objects.begin() + objectToRemove);
				++m_objectsRemoved;
			}
			return pObject;
		}
		return GameObjectPtr();
	}

	GameComponentPtr GetRandomComponent()
	{
		std::lock_guard<std::shared_mutex> lock(m_componentsMutex);
		if (!m_components.empty())
		{
			int              componentToRemove = std::rand() % m_components.size();
			GameComponentPtr pCpnt             = m_components[componentToRemove];
			m_components.erase(m_components.begin() + componentToRemove);
			++m_componentsRemoved;
			return pCpnt;
		}
		return GameComponentPtr();
	}

	void Stop()
	{
		while (m_runningTasks > 0 && m_clock.getElapsedTime().asMilliseconds() < timeOut) {}
		assert(m_runningTasks == 0 && m_objects.empty() && m_components.empty());
		m_stop = true; // avoid infinite loops but if they need to be stopped there is a problem
	}

private:
	static constexpr int s_numOfWorkingThreadsByAction = 100;
	static constexpr int s_numObjectsToAddByThread     = 100;
	static constexpr int s_numObjectsToAdd             = s_numOfWorkingThreadsByAction * s_numObjectsToAddByThread;
	static constexpr int s_numComponentsToAddByThread  = 100;
	static constexpr int s_numComponentsToAdd          = s_numOfWorkingThreadsByAction * s_numComponentsToAddByThread;

	std::atomic<int> m_runningTasks{0};
	std::atomic<int> m_objectsAdded{0};
	std::atomic<int> m_objectsRemoved{0};
	std::atomic<int> m_componentsAdded{0};
	std::atomic<int> m_componentsRemoved{0};

	std::shared_mutex             m_objectsMutex;
	std::vector<GameObjectPtr>    m_objects;
	std::shared_mutex             m_componentsMutex;
	std::vector<GameComponentPtr> m_components;

	sf::Int32 timeOut;
	sf::Clock m_clock;
	bool      m_stop = false;
};

class UnitTest
{
public:
	UnitTest()  = default;
	~UnitTest() = default;

	void Run()
	{
		ObjectsManagerTests(9999).Run();

		// EventSystem
		if (false)
		{
			std::srand((unsigned int)std::time(NULL));

			for (int i = 0; i < 100; i++)
			{
				TaskManager::Get().AddTask([this]() {
					for (int j = 0; j < 100; j++)
					{
						int a = std::rand() % 4;
						switch (a)
						{
						case 0:
							{
								m_eventSystem.AddQueuedEventCallback(this, std::rand() % 4, []() {});
							}
							break;
						case 1:
							{
								m_eventSystem.RemoveQueuedEventCallback(this, std::rand() % 4);
							}
							break;
						case 2:
							{
								m_eventSystem.QueueEvent(std::rand() % 4);
							}
							break;
						default:
							{
								m_eventSystem.ProcessEvents();
							}
							break;
						}
					}
				});
			}
		}
	}

private:
	EventSystem m_eventSystem;
};
} // namespace Logicraft