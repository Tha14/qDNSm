#pragma once

/*
 * The one place the version is written down.
 *
 * It was hard-coded twice - once in the menu header, once on the About screen - and the two copies
 * disagreed: the header said 0.92 while About said 1.1. Two copies of one fact drift, and that was
 * the proof.
 *
 * Two forms because the two callers need two forms. The menu header draws a wide string with its own
 * "v" prefix; the About body is a narrow literal inside a larger block. Adjacent literals concatenate
 * at compile time, so both can be built from the same number without a runtime format.
 *
 * Changing the version means editing this file and nothing else.
 */
#define QDNSM_VERSION        L"1.2"
#define QDNSM_VERSION_ASCII  "1.2"
