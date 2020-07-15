#include "CommandGraph.h"
#include "Device.h"
#include <algorithm>
#include <execution>

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

void CommandGraph::Execute(Device* device)
{
	// TODO : Compare performance difference with the worker pool

	// Signal to all the threads that a new execute function was called
	mCurrentDevice = device;
	SetEvent(mStartedExecutionEvent);
}