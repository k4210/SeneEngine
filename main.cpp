#include "stdafx.h"
#include "utils/base_system.h"
#include "utils/base_app.h"
#include "stat/stat.h"
#include "common_msg.h"

#include "systems/renderer/renderer_interface.h"
#include "systems/render_data_manager/render_data_manager_interface.h"
#include "systems/data_source/data_source_interface.h"
#include "systems/gameplay/gameplay_interface.h"
#include "systems/statistics/statistics_system.h"

IF_DO_LOG(LogCategory main_log("main");)
//IF_DO_STAT(static Stat::Id stat_app_tick("app", "ticks_per_frame", Stat::EMode::PerFrame));
//IF_DO_STAT(static Stat::Id stat_app_msg("app", "pending_msg", Stat::EMode::Override));

int run_application(BaseApp& pSample, UINT width, UINT height);

class SceneEngine : public BaseApp
{
	std::vector<std::unique_ptr<IBaseSystem>> systems_;
	LockFreeQueue_SingleConsumer<CommonMsg::Message, 32> msg_queue_;

protected:
	void OnInit(HWND hWnd, UINT width, UINT height) override;
	void Tick() override 
	{ 
		//IF_DO_STAT(stat_app_tick.PassValue(1));
		while (auto opt_msg = msg_queue_.Pop())
		{
			//IF_DO_STAT(stat_app_msg.PassValue(msg_queue_.Num()));
			//STAT_TIME_SCOPE(app, broadcast);
			for (auto& system : systems_)
			{
				assert(system);
				system->ReceiveCommonMessage(*opt_msg);
			}
		}
		std::this_thread::yield();
	}
	void OnDestroy() override;
	void ToggleFullscreenWindow() override;
	void ReceiveMsgToBroadcast(CommonMsg::Message msg) override
	{
		msg_queue_.Enqueue(std::move(msg));
	}
};

int main()
{
	SceneEngine se;
	return run_application(se, 1280, 720);
}

void SceneEngine::OnInit(HWND hWnd, UINT width, UINT height)
{
	LOG(main_log, ELog::Display, "OnInit");
	systems_.emplace_back(IRenderer::CreateSystem(hWnd, width, height, *this));
	systems_.emplace_back(IRenderDataManager::CreateSystem());
	systems_.emplace_back(IGameplay::CreateSystem());
	IF_DO_STAT(systems_.emplace_back(Statistics::CreateSystem()));

	for (auto& system : systems_)
	{
		assert(system);
		LOG(main_log, ELog::Display, "starting {}", system->GetName());
		system->Start();
	}
}

void SceneEngine::OnDestroy()
{
	LOG(main_log, ELog::Display, "OnDestroy");
	for (auto& system : systems_)
	{
		assert(system);
		CLOG(system->IsRunning(), main_log, ELog::Warning, "alive {}", system->GetName());
		if (system->IsRunning())
		{
			LOG(main_log, ELog::Display, "stopping {}", system->GetName());
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