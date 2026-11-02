#pragma once
#include <vsh/paf.hpp>
#include <vsh/vshcommon.hpp>
#include "Utils/SystemCalls.hpp"

inline void ShowSuccessNotification(const std::wstring& message)
{
    paf::View* systemPlugin = paf::View::Find("system_plugin");
    if (!systemPlugin)
        return;

    paf::SurfacePtr texture(nullptr);

    paf::View* explorePlugin = paf::View::Find("explore_plugin");
    if (explorePlugin)
        texture = explorePlugin->GetTexture("tex_check_ws");

    if (texture.ptr == nullptr)
    {
        paf::View* exploreFullPlugin = paf::View::Find("explore_plugin_full");
        if (exploreFullPlugin)
            texture = exploreFullPlugin->GetTexture("tex_check_ws");
    }

    if (texture.ptr == nullptr)
        texture = systemPlugin->GetTexture("tex_notification_info");

    if (texture.ptr == nullptr)
        return;

    vsh::ShowNotificationBySurfaceTexture(texture, message.c_str());
    systemPlugin->PlaySound("snd_system_ok");
}

class Helpers
{
public:
   Helpers();

   void OnUpdate();

public:
   paf::View* m_SystemPlugin{};
   paf::PhWidget* m_PageAutoOffGuide{};

   bool m_IsHen{};

   /*
    * Set by the worker thread: the console is running something other than the XMB, so a game
    * is up. The pad hook reads it to stop masking the buttons for whatever is in front.
    */
   volatile bool m_GameRunning{};
};

extern Helpers g_Helpers;