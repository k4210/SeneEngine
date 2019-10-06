#include "stdafx.h"
#include "scene_thread.h"

void SceneThread::Tick()
{
	//InsertPendingNodes

	std::vector<ST_MSG_AddMesh> mesh_to_add;
	std::vector<ST_MSG_RemoveMesh> mesh_to_remove;
	auto handle_msg = [&]()
	{
		ST_MSG msg;
		while (message_queue_.try_dequeue(msg))
		{
			const EST_MSG msg_type = static_cast<EST_MSG>(msg.index());
			switch (msg_type)
			{
			case EST_MSG::AddMesh:
				mesh_to_add.push_back(std::get<ST_MSG_AddMesh>(msg)); break;
			case EST_MSG::RemoveMesh:
				mesh_to_remove.push_back(std::get<ST_MSG_RemoveMesh>(msg));	break;
			case EST_MSG::AddStaticNode:
				pending_add_static_node_.push_back(std::get<ST_MSG_AddStaticNode>(std::move(msg)));	break;
			case EST_MSG::RemoveStaticNode:
			{
				const auto removed = static_nodes_.erase(std::get<ST_MSG_RemoveStaticNode>(msg).node_id);
				assert(removed);
				break;
			}
			default: assert(false); break;
			}
		}
	};

	handle_msg();
	const FenceValue current_batch = tick_index_.fetch_add(1);
	handle_msg(); // called to ensure that no msg was left unhandled with previous frame number

	const FenceValue unfinished_RT = renderer_common_.GetLastUnfinishedFrameRT();
	const FenceValue unfinished_MT = renderer_common_.GetLastUnfinishedFrameMT();

	//submit new SceneNodes buffer
	//send msg to render thread

	//wait for rt and moveable
	
	//submit changes in mesh table
	//wait for gpu..
}
