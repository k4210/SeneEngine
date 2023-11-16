#include "stdafx.h"
#include "statistics_system.h"
#include "systems/renderer/renderer_interface.h"
#include "imgui.h"

#if DO_STAT
namespace Statistics
{
	IF_DO_LOG(LogCategory stat_log("stat");)
	System* g_instance = nullptr;

	IBaseSystem* CreateSystem()
	{
		g_instance = new System();
		return g_instance;
	}

	void System::Destroy()
	{
		g_instance = nullptr;
	}

	void PassStat(uint32 index, Stat::EMode mode, double value)
	{
		g_instance->ReceiveStat(index, mode, value);
	}

	void System::ThreadInitialize()
	{
		Stat::Id::SetHandleStatsFunction(&PassStat);
		enabled_.set(); // ?
	}

	void System::ThreadCleanUp()
	{
		Stat::Id::SetHandleStatsFunction(nullptr);
	}

	void System::ReceiveStat(uint32 index, Stat::EMode mode, double value)
	{
		auto& var = buffers_.GetActive()[index];
		var.counter++;
		switch (mode)
		{
		case Stat::EMode::Accumulate:
		case Stat::EMode::PerFrame:
			var.value += value;
			break;
		case Stat::EMode::Override:
			var.value = value;
			break;
		}
	}

	void System::operator()(CommonMsg::Message msg)
	{
		if (const CommonMsg::Frame* frame = std::get_if<CommonMsg::Frame>(&msg))
		{
			buffers_.FlipActive();

			std::vector<std::string> text;

			auto& full_bufff = buffers_.GetInactive();
			const std::span<const Stat::Details> details = Stat::Id::AllStatsDetails();
			for (uint32 idx = 0; idx < details.size(); idx++)
			{
				if (!enabled_[idx])
					continue;
				const Stat::Details& detail = details[idx];
				auto& elem = full_bufff[idx];
				const double value = elem.value.load(std::memory_order_relaxed);
				const uint32 counter = elem.counter.load(std::memory_order_relaxed);

				std::string str = std::format("{}:{} {} / {}", detail.group, detail.name, value, counter);
				text.push_back(std::move(str));

				if (detail.mode == Stat::EMode::PerFrame)
				{
					elem.value.store(0.0f, std::memory_order_relaxed);
					elem.counter.store(0, std::memory_order_relaxed);
				}
			}

			auto draw_hud = [text = std::move(text), delta_ms = Utils::ToMiliseconds(frame->delta)]()
			{
				ImGui::Begin("Stats");
				ImGui::Text("frame %f ms", delta_ms);
				for (const auto& it : text)
				{
					ImGui::Text(it.data());
				}
				if (ImGui::Button("Show Render"))
				{
					g_instance->EnqueueMsg(StartDisplay{ "renderer" });
				}
				if (ImGui::Button("Hide Render"))
				{
					g_instance->EnqueueMsg(StopDisplay{ "renderer" });
				}
				ImGui::End();
			};

			IRenderer::EnqueueMsg(IRenderer::RT_MSG_RegisterDrawHud{ std::move(draw_hud) });
		}
	}

	void SetByGroup(std::string_view group_name, std::bitset<Stat::kMaxSupportedStats>& enabled, bool val)
	{
		const auto details = Stat::Id::AllStatsDetails();
		for (uint32 idx = 0; idx < details.size(); idx++)
		{
			if (group_name == details[idx].group)
			{
				enabled[idx] = val;
			}
		}
	}

	void System::operator()(StartDisplay msg)
	{
		SetByGroup(msg.group_name, enabled_, true);
	}

	void System::operator()(StopDisplay msg)
	{
		SetByGroup(msg.group_name, enabled_, false);
	}

}
#endif