#include "CommandGraph.h"
#include "Device.h"
#include "../Core/Log.h"

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
	mStartWorkEvents(num_workers)
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
		auto& cl = mCommandLists.emplace_back();
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
					node->body(mCommandLists[worker_id].Get(), worker_id);
					if (work_index == 0)
					{
						for (Node* dependent_node : node->dependent_nodes)
						{
							int ready_dependencies = dependent_node->num_ready_dependencies.fetch_add(1) + 1;
							if (ready_dependencies == dependent_node->dependencies.size())
							{
								std::scoped_lock(mLock);
								mNextWorkQueue.push_back(dependent_node);
							}
						}
					}
				}

				mCommandLists[worker_id]->Close();
				SetEvent(mWorkerFinishedEvents[worker_id]);
			}
		});
	}
}

void CommandGraph::AddNode(std::string name, std::function<void(ID3D12GraphicsCommandList*, uint32_t)> node_body, std::vector<std::string> dependencies, uint32_t repeats)
{
	ConstructionNode node;
	node.body = node_body;
	node.repeats = repeats;
	node.dependencies = dependencies;
	
	mNamedNodes[name] = node;
}

void CommandGraph::Build(Device* device)
{
	using namespace std;

	mNodes = new Node[mNamedNodes.size()];

	unordered_map<string, size_t> name_index_map;
	size_t node_idx = 0;
	for (auto& [name, tmp_node] : mNamedNodes)
	{
		name_index_map[name] = node_idx;

		mNodes[node_idx].body = tmp_node.body;
		mNodes[node_idx].repeats = tmp_node.repeats;

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

					node_ptr->dependencies.push_back(dependency_ptr);
					dependency_ptr->dependent_nodes.push_back(node_ptr);
				}
			}
		}
	}

	// Reserve max capacity
	mWorkQueue = new WorkItem[mNodesCount]; 

	// No longer necessary
	mNamedNodes.clear();
}

void CommandGraph::Execute(Device* device, ID3D12PipelineState* initial_state)
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
	{
		mCommandAllocators[i]->Reset();
		// TODO : See what to do with initial states
		mCommandLists[i]->Reset((*mCommandAllocators[i]).Get(), initial_state);
	}

	while (mWorkQueueSize > 0)
	{
		mCurrentItemIndex = 0;

		for (HANDLE event : mStartWorkEvents) SetEvent(event);
		
		WaitForMultipleObjects(mWorkerFinishedEvents.size(), mWorkerFinishedEvents.data(), true, INFINITE);
		queue->ExecuteCommandLists(mCommandLists.size(), mRawCommandLists);

		mWorkQueueSize = 0;
		for (Node* node : mNextWorkQueue)
		{
			WorkItem& item = mWorkQueue[mWorkQueueSize++];
			item.node_ptr = node;
			item.current_work_index = 0;
		}
		mNextWorkQueue.resize(0);
	}

	// Signal the fenche and advance the buffered resources
	device->SignalQueueWork(mType);
	sCurrentResourceBufferIndex++;

	// If we are going to roll back to the first allocator, then we need to wait
	if (sCurrentResourceBufferIndex % kResourceBufferCount == 0)
	{
		device->WaitForQueue(mType);
	}
}

CommandGraph::~CommandGraph()
{
	delete[] mNodes;
	delete[] mWorkQueue;
	delete[] mRawCommandLists;

	for (HANDLE event : mStartWorkEvents) CloseHandle(event);
	for (HANDLE event : mWorkerFinishedEvents) CloseHandle(event);
}