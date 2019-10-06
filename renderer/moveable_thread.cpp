#include "stdafx.h"
#include "moveable_thread.h"

void MoveableThread::Tick()
{
	//wait for GPU to finish frame -2

	const FenceValue last_loaded_batch = renderer_common_.GetLastLoadedBatch();

	for (auto iter = pending_add_.begin(); iter != pending_add_.end();)
	{
		if (iter->needed_batch <= last_loaded_batch)
		{
			instances_.insert_or_assign(iter->instance_id, iter->instance);
			iter = pending_add_.erase(iter);
		}
		else
		{
			iter++;
		}
	}

	auto add_instance = [&](MT_MSG_Add msg)
	{
		if (msg.needed_batch <= last_loaded_batch)
		{
			instances_.insert_or_assign(msg.instance_id, msg.instance);
		}
		else
		{
			pending_add_.push_back(msg);
		}
	};

	auto remove_instance = [&](MT_MSG_Remove msg)
	{
		if (0 != instances_.erase(msg.instance_id))
			return;
		for (auto iter = pending_add_.begin(); iter != pending_add_.end(); iter++)
		{
			if (iter->instance_id == msg.instance_id)
			{
				pending_add_.erase(iter);
				return;
			}
		}
		assert(false);
	};

	auto update_instance = [&](MT_MSG_Update msg)
	{
		auto it = instances_.find(msg.instance_id);
		if (it != instances_.end())
		{
			it->second.bounding_sphere_world_space_ = msg.bounding_sphere_ws;
			it->second.model_to_world = msg.model_to_world;
			return;
		}
		for (auto& add_msg : pending_add_)
		{
			if (add_msg.instance_id == msg.instance_id)
			{
				add_msg.instance.bounding_sphere_world_space_ = msg.bounding_sphere_ws;
				add_msg.instance.model_to_world = msg.model_to_world;
				return;
			}
		}
		assert(false);
	};

	auto handle_msg = [&]()
	{
		MT_MSG msg;
		while (message_queue_.try_dequeue(msg))
		{
			const EMT_MSG msg_type = static_cast<EMT_MSG>(msg.index());
			switch (msg_type)
			{
			case EMT_MSG::Add:		add_instance   (std::get<MT_MSG_Add>   (msg));	break;
			case EMT_MSG::Update:	update_instance(std::get<MT_MSG_Update>(msg));	break;
			case EMT_MSG::Remove:	remove_instance(std::get<MT_MSG_Remove>(msg));	break;
			default: assert(false); break;
			}
		}
	};

	handle_msg();
	const FenceValue current_frame_index = tick_index_.fetch_add(1);
	handle_msg();

	//
}