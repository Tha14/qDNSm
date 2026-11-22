#pragma once

#include "../OpenSpy/OpenSpyProtocol.hpp"

enum OpenSpyPatchStart
{
	OpenSpyPatch_Started,
	OpenSpyPatch_NoGame,
	OpenSpyPatch_AlreadyRunning,
	OpenSpyPatch_StartFailed
};

OpenSpyPatchStart StartOpenSpyPatch();
bool IsOpenSpyModuleAvailable();
bool IsOpenSpyPatchBusy();
void OpenSpyModule_OnFrame(bool menuOpen);
void PollOpenSpyPatch();
void StopOpenSpyPatch();

void ShowOpenSpyPatchResult(OpenSpyPatchOutcome outcome);
