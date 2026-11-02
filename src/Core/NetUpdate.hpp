#pragma once

/*
 * Downloads and applies the managed DNS list and, when supported games are enabled, the
 * server game catalogue.
 *
 * qDNSmNetLib performs the fetch, validation, and file updates on a worker thread.
 * PollNetUpdate() reloads the resulting config and notifies the menu on the XMB frame
 * thread, so the menu's config and panel reads never race a writer.
 */
bool StartNetUpdate();
bool IsNetUpdateBusy();
void PollNetUpdate();
void StopNetUpdate();
