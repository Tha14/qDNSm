#include "Dialog.hpp"

#if defined(QDNSM_PSL1GHT)

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/stat.h>

#include <ppu-lv2.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <sysutil/video.h>
#include <sysutil/msg.h>
#include <sysutil/sysutil.h>
#include <io/pad.h>
#include <lv2/process.h>

namespace
{
    /* From vendor/PSL1GHT/samples/sys/save/include/rsxutil.h. */
    const u32 kCmdBufferSize = 0x100000;        /* CB_SIZE */

    /* RSX host memory. The save sample asks for 32MB, but that sample has only
     * ever been verified in RPCS3. Apollo - whose EBOOT is known to work on real
     * hardware - hands tiny3d just 1MB. We draw a background and nothing else,
     * so 8MB is ample, and asking a real PS3 for a large block is a plausible way
     * to hang the console for a reason an emulator never notices. */
    const u32 kHostSize      = 8 * 1024 * 1024;


    /* SysUtil needs a surface to draw on even with opaque dialogs - with RSX
     * skipped entirely the dialogs are created (audibly) but nothing appears.
     *
     * What it must not be is a single surface: one buffer tears, because the GPU
     * scans it while SysUtil composites into it. See InitDisplayBuffer.
     *
     * False skips the whole video bring-up. It works for the audio path and the
     * non-interactive install, but shows no dialog, so it is not the shipping
     * configuration. */
    const bool kOwnDisplay = true;

    /* Loop iterations between presentations. The wait loop sleeps ~1ms per
     * iteration, so 100 is roughly 10 frames a second.
     *
     * SysUtil composites its dialog into our framebuffer, and we present that
     * buffer. At 60fps we were scanning out frames caught mid-composite, which
     * is what flickers the screen and saturates the dialog's text. Slowing the
     * presentation reduces how often a half-composited surface reaches the
     * display, while still feeding the pipeline often enough not to wedge. */
    /* A dialog that is never answered must not be able to hang the console. */
    const u32 kDialogTimeoutMs = 120000;

    /* Several display buffers, flipped in turn.
     *
     * The flicker is tearing: it starts redrawing from the top because the
     * display is scanning the buffer while SysUtil is still compositing into it.
     * One buffer cannot avoid that - something is always drawing into whatever
     * is being presented. Slowing the presentation only reduces how often it is
     * caught mid-update; it does not fix it.
     *
     * This is what samples/sys/save does (MAX_BUFFERS) and what apollo's tiny3d
     * does. We tried it once, but back when the dialogs were transparent and
     * composited over a background image - a different arrangement, and a
     * different symptom. Not yet tried with SysUtil drawing opaque dialogs. */
    const int kBufferCount = 3;
    void* g_hostAddr     = NULL;
    u32*  g_displayBuf[kBufferCount] = { NULL, NULL, NULL };
    u32   g_displayOffset[kBufferCount] = { 0, 0, 0 };
    int   g_current = 0;
    u32   g_displayPitch = 0;
    u32   g_displayW = 0;
    u32   g_displayH = 0;
    gcmContextData* g_context = NULL;
    bool  g_ready = false;

    /* Set by the SysUtil callback, polled by the wait loops. */
    int   g_button    = MSG_DIALOG_BTN_NONE;
    bool  g_answered  = false;
    bool  g_dialogOpen = false;

    /* Plain opaque black.
     *
     * The background image is gone: it only existed so the transparent dialogs
     * had something to composite over, and those no longer ask for it. */
    void ClearDisplay()
    {
        for (int b = 0; b < kBufferCount; b++)
        {
            if (g_displayBuf[b] == NULL)
                continue;

            for (u32 y = 0; y < g_displayH; y++)
            {
                u32* drow = g_displayBuf[b] + (u64)y * g_displayPitch / 4;
                for (u32 x = 0; x < g_displayW; x++)
                    drow[x] = 0xFF000000u;
            }
        }
    }

    bool InitDisplayBuffer(u32 width, u32 height, u32 pitch)
    {
        u32 size = pitch * height;

        for (int i = 0; i < kBufferCount; i++)
        {
            g_displayBuf[i] = (u32*)rsxMemalign(64, size);
            if (g_displayBuf[i] == NULL)
                return false;

            if (rsxAddressToOffset(g_displayBuf[i], &g_displayOffset[i]) != 0)
                return false;

            if (gcmSetDisplayBuffer(i, g_displayOffset[i], pitch, width, height) != 0)
                return false;
        }

        g_displayPitch = pitch;
        g_displayW = width;
        g_displayH = height;

        /* Opaque. Every pixel's top byte is the alpha channel, and a zero-alpha
         * framebuffer is transparent - the compositor blends it over the previous
         * frame instead of replacing it. */
        ClearDisplay();
        return true;
    }

    /* One presentation per iteration, continuously, for as long as a dialog is up.
     *
     * The flip sequence is the save sample's, verbatim:
     *
     *   if (gcmSetFlip (context, buffer) == 0) {
     *       rsxFlushBuffer (context);
     *       gcmSetWaitFlip (context);   // gate the RSX until the flip lands
     *   }
     *   while (gcmGetFlipStatus () != 0) usleep (200);
     *   gcmResetFlipStatus ();
     *
     * Two separate mistakes lived here for a long time and they masked each other.
     * Dropping gcmSetWaitFlip() left the RSX ungated, so the flip was queued but
     * never executed - a black screen even with the buffer painted solid red,
     * i.e. this process had never displayed anything at all. The wait condition
     * was also inverted (== 0 rather than != 0), so it spun until a flip *started*
     * and, once one had completed, sat at 0 forever - an unbounded spin that
     * froze the console solid, Guide button and all. Fixing one hid the other:
     * removing the freeze kept the fix for the black screen removed too.
     *
     * The presentation must also stay continuous. Presenting a single frame per
     * painted image and then stopping wedges the console outright - SysUtil needs
     * the surface kept fed while its dialog is up. */
    void PumpRsx()
    {
        /* sysUtilCheckCallback() services SysUtil and dispatches the dialog's
         * button press. Without it the loop spins forever: the dialog opens, it
         * renders, it looks interactive, and no press is ever delivered. This is
         * the one call vendor/PSL1GHT/samples/sys/msgdialog makes in its own wait
         * loop (do_flip(): sysUtilCheckCallback(); flip();). */
        /* Every iteration: this is what services SysUtil and dispatches the
         * button press. Never throttle this or input stops arriving. */
        sysUtilCheckCallback();

        if (!kOwnDisplay)
            return;

        if (!g_dialogOpen)
            return;          /* nothing open: do not compete with the XMB */

        if (g_context == NULL || g_displayBuf[g_current] == NULL)
            return;

        int next = (g_current + 1) % kBufferCount;

        if (gcmSetFlip(g_context, next) == 0)
        {
            rsxFlushBuffer(g_context);
            gcmSetWaitFlip(g_context);

            u32 spins = 0;
            const u32 kMaxSpins = 6000;      /* ~1.2s at usleep(200) */
            while (gcmGetFlipStatus() != 0 && spins++ < kMaxSpins)
                usleep(200);

            gcmResetFlipStatus();
            g_current = next;
        }
    }

    void OnButton(msgButton button, void* /*userData*/)
    {
        /* Note MSG_DIALOG_BTN_OK and MSG_DIALOG_BTN_YES are both 1 in the
         * msgButton enum, so the OK case below is already covered - do not add a
         * duplicate label, it will not compile.
         *
         * BTN_INVALID arrives when the dialog merely opens and must be ignored: a yes/no dialog does not
         * produce it, and mapping it to "yes" would silently turn an unrecognised
         * button into an install. BTN_INVALID arrives when the dialog merely
         * opens and must be ignored, or the wait loop exits before any press. */
        switch (button)
        {
            case MSG_DIALOG_BTN_YES:
                g_button = (int)button;
                g_answered = true;
                break;

            case MSG_DIALOG_BTN_NO:
            case MSG_DIALOG_BTN_ESCAPE:
            case MSG_DIALOG_BTN_NONE:
                g_button = (int)button;
                g_answered = true;
                break;

            default:
                break;
        }
    }

    void OnSysUtilEvent(u64 type, u64 param, void* /*userData*/)
    {
        (void)type;
        (void)param;
    }

    bool OpenDialog(int type, const char* text)
    {
        g_button = MSG_DIALOG_BTN_NONE;
        g_answered = false;
        g_dialogOpen = false;

        /* Repaint before every dialog, so whatever the previous dialog drew does
         * not stay on screen when the next one opens.
         *
         * These dialogs no longer set MSG_DIALOG_BKG_INVISIBLE, so SysUtil draws
         * its own opaque surface and does not composite onto ours. That was the
         * arrangement suspected of the ghosting - white text saturating toward
         * brighter white over time, as repeated compositing accumulated. */
        /* Repainting is now belt-and-braces rather than load-bearing, but it is
         * cheap and keeps the background correct on any dialog that does end up
         * transparent. */
        if (kOwnDisplay && g_displayBuf[0] != NULL)
            ClearDisplay();

        if (msgDialogOpen2((msgType)type, text, OnButton, NULL, NULL) == 0)
            g_dialogOpen = true;

        return g_dialogOpen;
    }

    void WaitForButton()
    {
        u32 waited = 0;
        while (!g_answered && waited < kDialogTimeoutMs)
        {
            PumpRsx();
            usleep(1000);
            waited += 1;
        }

        if (g_dialogOpen)
        {
            msgDialogAbort();
            usleep(100 * 1000);
            g_dialogOpen = false;
        }
    }
}

namespace Dialog
{
    bool VideoInit()
    {
        if (g_ready)
            return true;

        if (!kOwnDisplay)
        {
            ioPadInit(7);
            sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, OnSysUtilEvent, NULL);

            g_ready = true;
            return true;
        }

        videoState state;
        videoConfiguration config;
        videoConfiguration current;
        videoResolution res;

        g_hostAddr = memalign(1024 * 1024, kHostSize);
        if (g_hostAddr == NULL)
            return false;

        g_context = rsxInit(kCmdBufferSize, kHostSize, g_hostAddr);
        if (g_context == NULL)
        {
            free(g_hostAddr);
            g_hostAddr = NULL;
            return false;
        }

        if (videoGetState(0, 0, &state) != 0 || state.state != 0)
            return VideoFinish(), false;

        if (videoGetResolution(state.displayMode.resolution, &res) != 0)
            return VideoFinish(), false;

        /* Take the console's own pitch rather than assuming width * 4. Writing
         * rows at the wrong stride is the same class of fault as the ghosting
         * seen around the dialog: content landing where it should not. */
        memset(&current, 0, sizeof(current));
        videoGetConfiguration(0, &current, NULL);
        const u32 pitch = current.pitch ? current.pitch : (res.width * 4);

        /* Match the mode that is already running.
         *
         * This renegotiates the pipeline the XMB owns, and it was only ever
         * justified by the background image coming out corrupted without it.
         * That image is gone, so the justification has gone with it - worth
         * retesting without this call. */
        memset(&config, 0, sizeof(config));
        config.resolution = state.displayMode.resolution;
        config.format      = VIDEO_BUFFER_FORMAT_XRGB;
        config.pitch       = pitch;
        config.aspect      = state.displayMode.aspect;

        if (videoConfigure(0, &config, NULL, 0) != 0)
            return VideoFinish(), false;

        if (videoGetState(0, 0, &state) != 0 || state.state != 0)
            return VideoFinish(), false;

        gcmSetFlipMode(GCM_FLIP_VSYNC);
        gcmResetFlipStatus();

        /* The pad, and a sysutil event slot. SysUtil dialogs read button presses
         * through the pad, so with no pad attached there is no input source and
         * no press can be delivered. */
        ioPadInit(7);
        sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, OnSysUtilEvent, NULL);

        if (!InitDisplayBuffer(res.width, res.height, pitch))
        {
            VideoFinish();
            return false;
        }

        g_ready = true;
        return true;
    }

    void VideoFinish()
    {
        if (!kOwnDisplay)
        {
            g_ready = false;
            return;
        }

        sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
        ioPadEnd();

        if (g_context != NULL)
        {
            rsxFinish(g_context, 1);
            g_context = NULL;
        }
        for (int i = 0; i < kBufferCount; i++)
        {
            if (g_displayBuf[i] != NULL)
            {
                rsxFree(g_displayBuf[i]);
                g_displayBuf[i] = NULL;
            }
        }
        if (g_hostAddr != NULL)
        {
            free(g_hostAddr);
            g_hostAddr = NULL;
        }
        g_ready = false;
    }

    bool Available()
    {
        return g_ready;
    }

    Choice AskManage()
    {
        if (!Available())
            return Cancelled;

        /* MSG_DIALOG_DEFAULT_CURSOR_NO is part of apollo's yes/no type. */
        if (!OpenDialog(MSG_DIALOG_NORMAL |
                        MSG_DIALOG_BTN_TYPE_YESNO | MSG_DIALOG_DEFAULT_CURSOR_NO,
                        "You will be managing qDNSm with this app.\n\n"
                        "Choose an option:\n\n"
                        "Yes  =  Install\n"
                        "No   =  Uninstall"))
            return Cancelled;

        WaitForButton();

        if (g_button == MSG_DIALOG_BTN_YES)
            return Install;
        if (g_button == MSG_DIALOG_BTN_NO)
            return Uninstall;

        return Cancelled;
    }

    void BeginProgress(const char* title)
    {
        if (!Available())
            return;

        OpenDialog(MSG_DIALOG_NORMAL |
                   MSG_DIALOG_MUTE_ON | MSG_DIALOG_DISABLE_CANCEL_ON |
                   MSG_DIALOG_SINGLE_PROGRESSBAR, title);

        PumpRsx();
    }

    void SetProgress(const char* text)
    {
        if (!g_dialogOpen)
            return;

        msgDialogProgressBarSetMsg(MSG_PROGRESSBAR_INDEX0, text);
        msgDialogProgressBarReset(MSG_PROGRESSBAR_INDEX0);

        PumpRsx();
    }

    void EndProgress()
    {
        if (g_dialogOpen)
        {
            msgDialogAbort();
            g_dialogOpen = false;

            /* Let the abort settle before another dialog is opened. Without this
             * the completion message that follows straight after the progress bar
             * did not appear at all: WaitForButton() has always had this delay
             * (it is in apollo's show_dialog too), and EndProgress did not, so the
             * next msgDialogOpen2 raced the teardown. */
            usleep(100 * 1000);
        }
    }

    void Report(const char* title, const char* text)
    {
        if (!Available())
            return;

        /* MSG_DIALOG_BTN_TYPE_OK (32), the *type* flag - not MSG_DIALOG_BTN_OK,
         * which is a button *value* from the msgButton enum. Passing the value
         * here ORs a meaningless bit into the type field, and the dialog comes up
         * with its text but no button to press. */
        OpenDialog(MSG_DIALOG_NORMAL |
                   MSG_DIALOG_BTN_TYPE_OK, text);
        WaitForButton();
    }

    bool AskYesNo(const char* title, const char* text)
    {
        if (!Available())
            return false;

        if (!OpenDialog(MSG_DIALOG_NORMAL |
                        MSG_DIALOG_BTN_TYPE_YESNO | MSG_DIALOG_DEFAULT_CURSOR_NO,
                        text))
            return false;

        WaitForButton();
        return g_button == MSG_DIALOG_BTN_YES;
    }

    static int RebootSyscall()
    {
        unlink("/dev_hdd0/tmp/turnoff");
        lv2syscall4(379, 0x1200, 0, 0, 0);
        return_to_user_prog(int);
    }

    void Reboot()
    {
        RebootSyscall();
    }

    void Exit()
    {
        VideoFinish();
        sysProcessExit(0);
    }
}

#else   /* !QDNSM_PSL1GHT - SNC project and the host test build */

namespace Dialog
{
    bool VideoInit()      { return false; }
    void VideoFinish()    {}
    bool Available()      { return false; }
    Choice AskManage()    { return Cancelled; }
    void BeginProgress(const char*) {}
    void SetProgress(const char*) {}
    void EndProgress()    {}
    void Report(const char*, const char*) {}
    bool AskYesNo(const char*, const char*) { return false; }
    void Reboot()         {}
    void Exit()           {}
}

#endif