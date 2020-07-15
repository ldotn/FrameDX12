#pragma once
#include "../Core/stdafx.h"
#include "CommandListPool.h"
#include "Device.h"

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

	typedef Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> DXCommmandList;
	typedef Microsoft::WRL::ComPtr<ID3D12CommandAllocator> DXCommandAllocator;

	class CommandGraph
	{
	public:
		CommandGraph(size_t num_workers);

		// The node body returns an estimative of the number of commands added to the list and takes as input the command list and repeat index
		// TODO : Support nodes with dependencies from different queues, for now they need to be on the same queue
		//		  Having different queues means you are forced to do a ExecuteCommandLists and put a fence
		void AddNode(QueueType type, const char* name, std::function<uint32_t(ID3D12GraphicsCommandList*, uint32_t)> node_body, std::vector<const char*> dependencies, uint32_t repeats);
		
		// Does the following
		// - Finds all the nodes without dependencies and adds them to mStartingNodes
		// - Updates the nodes filling mDependentNodes with the pointers to nodes that depend on the node
		void Build(Device* device);

		// Does the following
		//  Reset mNumReadyDependencies to 0 on all nodes
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
		void Execute(Device * device);

		// Halts CPU execution until the whole graph has finished
		void WaitUntilFinished();
	private:
		// One per worker
		std::vector<DXCommmandList> mCommandLists; // Don't need to buffer command lists as you reset them at the start of Execute
		std::vector<BufferedResource<DXCommandAllocator>>    mCommandAllocators; // Need to be buffered as you may not be waiting between executes

		struct Node
		{
			uint32_t mRepeats;
			std::function<uint32_t(ID3D12GraphicsCommandList*)> mBody;
			QueueType mType;
			std::vector<Node*> mDependencies; // Think the vector is not needed, the count should be enough...
			std::vector<Node*> mDependentNodes;
			std::atomic<int> mNumReadyDependencies;
		};

		std::vector<Node*> mStartingNodes; // Nodes without dependencies
		std::vector<Node> mNodes;

		HANDLE mStartWorkEvent;
	};
}