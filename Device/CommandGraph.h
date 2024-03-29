#pragma once
#include "../Core/stdafx.h"
#include "Device.h"
#include "../Resource/BufferedResource.h"

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
		//			The way the CLs are filled might not make it necessary though
		// If an empty string is passed as the name, one is autogenerated in the form ___unnamed_node_#, where # is a counter
		//    That means you can't use a string like that for a name, though if you are actually calling a node that you need to get some sleep...
		void AddNode(std::string name, std::function<void(ID3D12GraphicsCommandList*)> init_body, std::function<void(ID3D12GraphicsCommandList*, uint32_t)> node_body, std::vector<std::string> dependencies, uint32_t repeats = 1);

		// Constructs all the internal structures needed to execute
		// Can only be called once
		void Build(Device* device);

		// Executes the graph dividing the work over multiple threads
		// Returns the workload id, so you're able to wait for this specific Execute to finish
		// ------------------------------------------------------------------------------------------------------------------
		// If the nodes have very different workloads this is not the most efficient as you are waiting for an entire graph level to finish before moving to the next
		// The problem with adding the nodes to the work queue as soon as their dependencies finish is that you need to force stop all threads, do and execute, reset the CLs and continue writing
		// For now let's keep it "simple" and do it like this
		//
		//		IMPORTANT NOTE : This doesn't wait for the GPU to finish nor advances the buffer index 
		//							Allocators are buffered but you do need to wait if you are using the same allocator again
		//
		// Side note: Repeats are executed counting down from the biggest index.
		uint64_t Execute(Device* device, ID3D12PipelineState* initial_state = nullptr);

		QueueType GetQueueType() const { return mType; }
	private:
		QueueType mType;

		// One per worker
		ID3D12CommandList** mRawCommandLists; // Non-owner array of pointers to the CLs
		std::vector<DXCommmandList> mCommandLists; // Don't need to buffer command lists as you reset them at the start of Execute
		std::vector<BufferedResource<DXCommandAllocator>> mCommandAllocators; // Need to be buffered as you may not be waiting between executes

		struct Node
		{
			std::string name; // used for the pix events
			uint32_t repeats;
			std::function<void(ID3D12GraphicsCommandList*, uint32_t)> body;
			std::function<void(ID3D12GraphicsCommandList*)> init;
			int num_dependencies;
			std::vector<Node*> dependent_nodes;
			std::atomic<int> num_ready_dependencies;
		};

		// References the nodes data that can be changed at runtime
		struct MutableNodeInfo
		{
			uint32_t* repeats;
		};
		std::unordered_map<std::string, MutableNodeInfo> mNodeInfo;

		std::vector<Node*> mStartingNodes; // Nodes without dependencies
		size_t mNodesCount;
		Node* mNodes;

		// Used during construction only, cleared after Build is called
		struct ConstructionNode
		{
			std::function<void(ID3D12GraphicsCommandList*, uint32_t)> body;
			std::function<void(ID3D12GraphicsCommandList*)> init;
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

	public:
		// Returns a struct with references to the mutable node info
		// Nullptrs are returned if the name wasn't found
		MutableNodeInfo operator[](const std::string& name);
	};
}