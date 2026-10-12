/*
 * On-screen prompts for the installer.
 *
 * There is no output at all without this: Ui::Notify is a no-op, so a user who
 * runs the installer and sees nothing happen has no way to learn what happened
 * or what to do about it. These are the console's own SysUtil dialogs - the same
 * ones the save-data browser uses - rather than anything drawn by hand.
 *
 * Two things had to be true for this to work, and both were found the hard way:
 *
 *   - SysUtil dialogs draw into a RSX context. An EBOOT is an ordinary process,
 *     not the XMB, so it has to bring up RSX and the video output itself before
 *     any dialog will appear. vendor/PSL1GHT/samples/sys/save does exactly this
 *     and is the reference for the sequence in VideoInit below.
 *
 *   - msgDialogOpen2 only starts a dialog. The callback fires asynchronously, so
 *     something has to stay running until it does. Waiting on a flag alone is
 *     enough here; the dialog is composited by the system, not by us.
 *
 * Everything is behind QDNSM_PSL1GHT. Under SNC, and on the host test build,
 * the whole module compiles to a stub that reports "no UI available" so the
 * non-interactive paths stay reachable and the tests keep working.
 */

#ifndef QDNSM_DIALOG_HPP
#define QDNSM_DIALOG_HPP

namespace Dialog
{
    enum Choice
    {
        Cancelled = 0,  /*!< ESC pressed, or the UI could not be brought up */
        Install,
        Uninstall
    };

    /*! Bring up RSX and video so SysUtil dialogs have somewhere to draw.
     *  Returns false when no UI is available - a host build, the SNC project,
     *  or a console that refused to start video. Callers must keep working
     *  when this returns false; it means "no prompts", not "do nothing". */
    bool VideoInit();

    void VideoFinish();

    /*! True when VideoInit succeeded and prompts can actually be shown. */
    bool Available();

    /*! The opening prompt: install/update, uninstall, or cancel. */
    Choice AskManage();



    /*! Progress dialog shown while the work happens. Safe to nest-free: one at
     *  a time, and EndProgress() must be called even if the work fails. */
    void BeginProgress(const char* title);
    void SetProgress(const char* text);
    void EndProgress();

    /*! Blocking single-button message. */
    void Report(const char* title, const char* text);

    /*! Blocking yes/no question. Returns true for yes. */
    bool AskYesNo(const char* title, const char* text);

    /*! Reboot the console. Does not return on success. */
    void Reboot();

    /*! Exit the process. Does not return. */
    void Exit();
}

#endif
