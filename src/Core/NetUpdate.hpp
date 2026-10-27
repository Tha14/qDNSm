#pragma once

/*
 * Downloads and applies the managed DNS list and, when supported games are enabled, the
 * server game catalogue.
 *
 * The network work runs on a worker thread. The worker fetches both files through
 * qDNSmNetLib's TLS job and validates them. PollNetUpdate() applies the validated
 * result on the XMB frame thread, so the menu's config and panel reads never race a
 * writer.
 */
bool StartNetUpdate();
bool IsNetUpdateBusy();
void PollNetUpdate();
void StopNetUpdate();
