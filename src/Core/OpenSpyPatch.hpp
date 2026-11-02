#pragma once

enum OpenSpyPatchStart
{
	OpenSpyPatch_Started,
	OpenSpyPatch_NoGame,
	OpenSpyPatch_Unsupported,
	OpenSpyPatch_AlreadyRunning,
	OpenSpyPatch_StartFailed
};

enum OpenSpyPatchOutcome
{
	OpenSpyPatch_Success,
	OpenSpyPatch_AlreadyPatched,
	OpenSpyPatch_NotFound,
	OpenSpyPatch_ReadFailed,
	OpenSpyPatch_WriteFailed,
	OpenSpyPatch_VerifyFailed,
	OpenSpyPatch_RollbackFailed,
	OpenSpyPatch_GameExited,
	OpenSpyPatch_TooManyMatches
};

OpenSpyPatchStart StartOpenSpyPatch();
bool IsOpenSpyPatchBusy();
void PollOpenSpyPatch();
void StopOpenSpyPatch();

void ShowOpenSpyPatchResult(OpenSpyPatchOutcome outcome);
