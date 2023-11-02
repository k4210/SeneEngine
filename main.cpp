#include "stdafx.h"
#include "utils/base_system.h"
#include "utils/base_app.h"

#include "systems/renderer/renderer_interface.h"
#include "systems/render_data_manager/render_data_manager_interface.h"
#include "systems/data_source/data_source_interface.h"
#include "systems/gameplay/gameplay_interface.h"

IF_DO_LOG(LogCategory main_log("main");)

int run_application(BaseApp& pSample, UINT width, UINT height);

class SceneEngine : public BaseApp
{
	std::vector<std::unique_ptr<IBaseSystem>> systems_;
protected:
	void OnInit(HWND hWnd, UINT width, UINT height) override;
	void Tick() override { std::this_thread::yield(); }
	void OnDestroy() override;
	void ToggleFullscreenWindow() override;
	void Broadcast(CommonMsg::Message msg) override
	{
		for (auto& system : systems_)
		{
			assert(system);
			system->HandleCommonMessage(msg);
		}
	}
};

int main()
{
	SceneEngine se;
	return run_application(se, 1280, 720);
}

void SceneEngine::OnInit(HWND hWnd, UINT width, UINT height)
{
	LOG(main_log, ELogPriority::Display, "OnInit");
	systems_.emplace_back(IRenderer::CreateSystem(hWnd, width, height));
	systems_.emplace_back(IRenderDataManager::CreateSystem());
	systems_.emplace_back(IGameplay::CreateSystem());

	for (auto& system : systems_)
	{
		assert(system);
		LOG(main_log, ELogPriority::Display, "starting {}", system->GetName());
		system->Start();
	}
}

void SceneEngine::OnDestroy()
{
	LOG(main_log, ELogPriority::Display, "OnDestroy");
	for (auto& system : systems_)
	{
		assert(system);
		CLOG(system->IsRunning(), main_log, ELogPriority::Warning, "alive {}", system->GetName());
		if (system->IsRunning())
		{
			LOG(main_log, ELogPriority::Display, "stopping {}", system->GetName());
			system->Stop();
		}
	}
	for (auto& system : systems_)
	{
		system->Destroy();
	}
	systems_.clear();
}

void SceneEngine::ToggleFullscreenWindow()
{
	IRenderer::RT_MSG_ToogleFullScreen msg;
	IRenderer::EnqueueMsg(msg);
}