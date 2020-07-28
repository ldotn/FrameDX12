#pragma once
#include "../Core/stdafx.h"
#include "Device.h"
#include "../Resources/BufferedResource.h"

namespace FrameDX12
{
	// Some pseudocode
	/*
	graph2.AddNode(Copy, "TransferData",
    [](cl)
    {
        cl.CopyVertex
    },
    {}
);

graph2.Build()
graph2.Execute()

graph.AddNode(Graphics, "Setup",
    [](cl)
    {
        cl.SetViewport()
        cl.Clear()
    },
    {}
);

graph.AddParallelNode(Graphics, "RenderObjects", objects.size(),
    [](cl, widx)
    {
        cl.Render(objects[widx])
    },
    {"Setup"}
);

graph.AddNode(Compute, "ComputeEnv",
    [](cl)
    {
        cl.Dispatch()
    },
    {"Setup"}
);

graph.AddNode(Compute, "PostPro",
    [](cl)
    {
        cl.Dispatch()
    },
    {"RenderObjects", "ComputeEnv"}
);

graph.Build();

// ...

graph.Execute();

Setup --> RenderObjects ----> PostPro --> Present
          ComputeEnv    --/
 	*/

	typedef ComPtr<ID3D12GraphicsCommandList> DXCommmandList;
	typedef ComPtr<ID3D12CommandAllocator> DXCommandAllocator;

	// TODO : Support nodes with dependencies from different queues, for now they need to be on the same queue
	//		  Having different queues means you are forced to do a ExecuteCommandLists and put a fence
	//		  And have different CLs
	// NOTE : THIS CLASS IS NOT THREAD SAFE
	class CommandGraph
	{
	public:
		CommandGraph(size_t num_workers, QueueType type, Device* device_ptr);
		~CommandGraph();
		
		// TODO : Take into account the estimated number of commands on the lists for CL reuse
		//			The way the CLs are filled might not make it neccessary though
		void AddNode(std::string name, std::function<void(ID3D12GraphicsCommandList*, uint32_t)> node_body, std::vector<std::string> dependencies, uint32_t repeats = 1);
		
		// Does the following
		// - Fill mNodes and mStartingNodes
		// - Update the nodes filling dependent_nodes with the pointers to nodes that depend on the node
		// - Update the nodes filling dependencies
		// - Clear mNameMap
		void Build(Device* device);

		// Does the following
		//  Reset mNumReadyDependencies to 0 on all nodes
		//  Reset all command lists and allocators (remember allocators are buffered so you only reset the current ones)
		//  Initialize the work queue with mStartingNodes (current index = mRepeats - 1 for each work item)
		//  next_work_queue = {}
		//  while work_queue.size > 0
		//		Signal mStartWorkEvent so all worker threads start executing
		//	[Worker thread]
		//		while !bExit
		//			Wait on mStartWorkEvent
		//			while current_item_index < work_queue.size
		//				item_index = current_item_index
		//				work_index = work_queue[item_index].current_work_index.fetch_sub(1);
		//				if work_index < 0 
		//					continue // Someone got a 0, so keep looping until that thread sets everything up
		//				else if work_index == 0
		//					item_index = current_item_index.fetch_add(1) // Advance so the next thread works in the next item
		//				
		//				node = work_queue[item_index].node
		//				Call mBody with the worker CL and c index
		//				if work_index == 0
		//					for dependent_node in node->mDependentNodes
		//						ready_dependencies = dependent_node->mNumReadyDependencies.fetch_add(1) + 1 
		//						if ready_dependencies == dependent_node->mDependencies.size
		//							Add work item for dependent_node to next_work_queue
		//   [Main thread]
		//		Wait for all threads to finish execution
		//		call ExecuteCommandLists
		//		work_queue = next_work_queue
		//		next_work_queue = {}
		// ------------------------------------------------------------------------------------------------------------------
		// If the nodes have very different workloads this is not the most efficient as you are waiting for an entire graph level to finish before moving to the next
		// The problem with adding the nodes to the work queue as soon as their dependencies finish is that you need to force stop all threads, do and execute, reset the CLs and continue writing
		// For now let's keep it "simple" and do it like this
		//
		//		IMPORTANT NOTE : This doesn't wait for the GPU to finish nor advances the buffer index 
		//							Allocators are buffered but you do need to wait if you are using the same allocator again
		//
		void Execute(Device * device, ID3D12PipelineState* initial_state = nullptr);
	private:
		QueueType mType;

		// One per worker
		ID3D12CommandList** mRawCommandLists; // Non-owner array of pointers to the CLs
		std::vector<DXCommmandList> mCommandLists; // Don't need to buffer command lists as you reset them at the start of Execute
		std::vector<BufferedResource<DXCommandAllocator>> mCommandAllocators; // Need to be buffered as you may not be waiting between executes

		struct Node
		{
			uint32_t repeats;
			std::function<void(ID3D12GraphicsCommandList*, uint32_t)> body;
			std::vector<Node*> dependencies; // Think the vector is not needed, the count should be enough...
			std::vector<Node*> dependent_nodes;
			std::atomic<int> num_ready_dependencies;
		};

		std::vector<Node*> mStartingNodes; // Nodes without dependencies
		size_t mNodesCount;
		Node* mNodes;

		// Used during construction only, cleared after Build is called
		struct ConstructionNode
		{
			std::function<void(ID3D12GraphicsCommandList*, uint32_t)> body;
			std::vector<std::string> dependencies;
			uint32_t repeats;
		};
		std::unordered_map<std::string, ConstructionNode> mNamedNodes;

		struct WorkItem
		{
			Node* node_ptr;
			std::atomic<int> current_work_index = 0;
		};
		WorkItem* mWorkQueue;
		std::vector<Node*> mNextWorkQueue;

		std::vector<HANDLE> mStartWorkEvents;
		std::vector<HANDLE> mWorkerFinishedEvents;
		std::vector<std::thread> mWorkers;
		std::atomic<int> mCurrentItemIndex;
		size_t mWorkQueueSize;
		std::mutex mLock;
		bool mCloseWorkers;
	};
}