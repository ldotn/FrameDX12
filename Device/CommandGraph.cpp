#include "CommandGraph.h"
#include "Device.h"
#include "../Core/Log.h"
#include "../Resource/CommitedResource.h"

using namespace FrameDX12;

/*void CommandNode::Execute(Device* DevicePtr)
{
	// Fetch the command list
	auto cl = DevicePtr->GetCLPool(mQueueType)->FetchCommandList(DevicePtr, mTargetSize);

	// Call user function
	mBody(cl);

	// Enqueue the CL
	DevicePtr->GetQueue(mQueueType)->

}*/

/*
void CommandGraph::InternalNode::Execute(CommandGraph* graph)
{
	while (true)
	{
		// Wait for the signal to start executing
		WaitForSingleObject(graph->mStartedExecutionEvent, INFINITE);

		// Wait for dependencies
		if (mDependencies.size() > 0)
		{
			WaitForMultipleObjects(mDependencies.size(), mDependencies.data(), TRUE, INFINITE);
		}

		// Fetch the command list
		auto cl = graph->mCurrentDevice->GetCLPool(mQueueType)->FetchCommandList(graph->mCurrentDevice, mCommandCountGuess);

		// Execute the body
		// TODO : Also execute this in parallel
	problema : no tiene porque ser paralelo todo el body
			ej 
		setup y render de objetos, render de objetos es la unica parte paralela
		podes meter el setup en otro nodo pero es un sync al pedo porque es algo que no puede ser en paralelo con el render
		for (uint32_t i = 0; i < mParallelWorkCount; i++)
		{
			mBody(i, cl);
		}

		// Add the command list to the queue
		cl->Close();
		// TODO : Batch all the CLs from the same graph level and execute them on the same call
		ID3D12CommandList* lists[] = { cl.get() };
		graph->mCurrentDevice->GetQueue(mQueueType)->ExecuteCommandLists(1, lists);

		// Signal that the node finished
		SetEvent(mFinishEvent);
	}
}
*/

CommandGraph::CommandGraph(size_t num_workers, QueueType type, Device* device_ptr) :
	mType(type),
	mWorkerFinishedEvents(num_workers),
	mStartWorkEvents(num_workers),
	mCloseWorkers(false),
	mCommandLists(num_workers)
{
	//mStartWorkEvent = CreateEvent(NULL,FALSE,FALSE,NULL);

	mRawCommandLists = new ID3D12CommandList * [num_workers];
	for (size_t worker_id = 0; worker_id < num_workers; worker_id++)
	{
		// Create the allocator
		auto& alloc = mCommandAllocators.emplace_back([&](uint8_t)
		{
			Microsoft::WRL::ComPtr<ID3D12CommandAllocator> new_alloc;
			LogCheck(device_ptr->GetDevice()->CreateCommandAllocator((D3D12_COMMAND_LIST_TYPE)type, IID_PPV_ARGS(new_alloc.GetAddressOf())), LogCategory::Error);
			return new_alloc;
		});		

		// Create the DX command list
		auto& cl = mCommandLists[worker_id];
		LogCheck(device_ptr->GetDevice()->CreateCommandList(
			0,
			(D3D12_COMMAND_LIST_TYPE)type,
			(*alloc).Get(), // Associated command allocator
			nullptr, // TODO : Do something with this!
			IID_PPV_ARGS(cl.GetAddressOf())), LogCategory::Error);
		cl->Close();
		mRawCommandLists[worker_id] = cl.Get();

		mWorkerFinishedEvents[worker_id] = CreateEvent(NULL, FALSE, FALSE, NULL);
		mStartWorkEvents[worker_id] = CreateEvent(NULL, FALSE, FALSE, NULL);

		mWorkers.emplace_back([this, worker_id]()
		{
			while (true)
			{
				WaitForSingleObject(mStartWorkEvents[worker_id], INFINITE);

				if (mCloseWorkers)
					break;

				Node* current_node = nullptr;
				while (true)
				{
					int item_index = mCurrentItemIndex;

					if (item_index >= mWorkQueueSize)
						break;

					int work_index = mWorkQueue[item_index].current_work_index.fetch_sub(1);

					if (work_index < 0)
						continue; // Someone got a 0, so keep looping until that thread sets everything up
					else if (work_index == 0)
						item_index = mCurrentItemIndex.fetch_add(1); // Advance so the next thread works in the next item

					Node* node = mWorkQueue[item_index].node_ptr;
					
					if (node != current_node)
					{
						if (current_node) PIXEndEvent(mCommandLists[worker_id].Get());

						current_node = node;
						PIXBeginEvent(mCommandLists[worker_id].Get(), 0, current_node->name.c_str());
						if(node->init) node->init(mCommandLists[worker_id].Get());
					}

					if(node->body) node->body(mCommandLists[worker_id].Get(), work_index);

					if (work_index == 0)
					{
						for (Node* dependent_node : node->dependent_nodes)
						{
							int ready_dependencies = dependent_node->num_ready_dependencies.fetch_add(1) + 1;
							if (ready_dependencies == dependent_node->num_dependencies)
							{
								std::scoped_lock(mLock);
								mNextWorkQueue.push_back(dependent_node);
							}
						}

						if (current_node) PIXEndEvent(mCommandLists[worker_id].Get());
					}
				}
				if (current_node) PIXEndEvent(mCommandLists[worker_id].Get());
				mCommandLists[worker_id]->Close();
				SetEvent(mWorkerFinishedEvents[worker_id]);
			}
		});
	}
}

void CommandGraph::AddNode(std::string name, std::function<void(ID3D12GraphicsCommandList*)> init_body, std::function<void(ID3D12GraphicsCommandList*, uint32_t)> node_body, std::vector<std::string> dependencies, uint32_t repeats)
{
	if (name.empty())
	{
		name = "___unnamed_node_" + std::to_string(mNamedNodes.size());
	}

	ConstructionNode node;
	node.init = init_body;
	node.body = node_body;
	node.repeats = repeats;
	node.dependencies = dependencies;
	
	LogAssert(mNamedNodes.find(name) == mNamedNodes.end(), LogCategory::Error);

	mNamedNodes[name] = node;
}

void CommandGraph::Build(Device* device)
{
	using namespace std;

	mNodesCount = mNamedNodes.size();
	mNodes = new Node[mNodesCount];
	mWorkQueue = new WorkItem[mNodesCount]; // Reserve max capacity

	unordered_map<string, size_t> name_index_map;
	size_t node_idx = 0;
	for (auto& [name, tmp_node] : mNamedNodes)
	{
		name_index_map[name] = node_idx;

		mNodes[node_idx].body = tmp_node.body;
		mNodes[node_idx].init = tmp_node.init;
		mNodes[node_idx].repeats = tmp_node.repeats;
		mNodes[node_idx].name = name;
		mNodes[node_idx].num_dependencies = tmp_node.dependencies.size();

		++node_idx;
	}

	for (auto& [name, tmp_node] : mNamedNodes)
	{
		Node* node_ptr = &mNodes[name_index_map[name]];
		if (tmp_node.dependencies.size() == 0)
		{
			mStartingNodes.push_back(node_ptr);
		}
		else
		{
			for (auto& dependency : tmp_node.dependencies)
			{
				auto name_index = name_index_map.find(dependency);

				if(LogAssertAndContinue(name_index != name_index_map.end(), LogCategory::Error))
				{
					Node* dependency_ptr = &mNodes[name_index->second];

					dependency_ptr->dependent_nodes.push_back(node_ptr);
				}
			}
		}
	}

	// No longer necessary
	mNamedNodes.clear();
}

uint64_t CommandGraph::Execute(Device* device, ID3D12PipelineState* initial_state)
{
	using namespace std;
	using namespace fpp;

	// TODO : Support nodes with dependencies from different queues, for now they need to be on the same queue
	//		  Having different queues means you are forced to do a ExecuteCommandLists and put a fence
	ID3D12CommandQueue * queue = device->GetQueue(mType);

	for (size_t i = 0; i < mNodesCount; ++i) mNodes[i].num_ready_dependencies = 0;

	mWorkQueueSize = mStartingNodes.size();
	for (size_t i = 0;i < mWorkQueueSize;++i)
	{
		mWorkQueue[i].node_ptr = mStartingNodes[i];
		mWorkQueue[i].current_work_index = mStartingNodes[i]->repeats - 1;
	}

	for (size_t i = 0;i < mCommandLists.size();++i)
		mCommandAllocators[i]->Reset();

	while (mWorkQueueSize > 0)
	{
		// TODO : See what to do with initial states
		for (size_t i = 0; i < mCommandLists.size(); ++i)
			mCommandLists[i]->Reset((*mCommandAllocators[i]).Get(), initial_state);

		mCurrentItemIndex = 0;

		for (HANDLE event : mStartWorkEvents) SetEvent(event);
		
		WaitForMultipleObjects(mWorkerFinishedEvents.size(), mWorkerFinishedEvents.data(), true, INFINITE);

		// Need to call execute between levels, otherwise you won't get the correct dependencies
		// Suppose node C depends on A and B
		// You add A to cl0, B to cl1, then C to cl0
		// If you try to execute cl0 and cl1 at the same time, you aren't respecting dependencies
		queue->ExecuteCommandLists(mCommandLists.size(), mRawCommandLists);

		for (auto cl : mCommandLists) UploadAllocator::ReleaseRegionsOnAllAllocators(cl.Get());

		mWorkQueueSize = 0;
		for (Node* node : mNextWorkQueue)
		{
			WorkItem& item = mWorkQueue[mWorkQueueSize++];
			item.node_ptr = node;
			item.current_work_index = node->repeats - 1;
		}
		mNextWorkQueue.resize(0);
	}

	// Signal the fence
	return device->SignalQueueWork(mType);
}

CommandGraph::~CommandGraph()
{
	mCloseWorkers = true;
	for (HANDLE event : mStartWorkEvents) SetEvent(event);
	for (auto& thread : mWorkers) thread.join();

	delete[] mNodes;
	delete[] mWorkQueue;
	delete[] mRawCommandLists;

	for (HANDLE event : mStartWorkEvents) CloseHandle(event);
	for (HANDLE event : mWorkerFinishedEvents) CloseHandle(event);
}