#pragma once
#include <vsh/paf.hpp>
#include <vsh/vshcommon.hpp>
#include "Utils/SystemCalls.hpp"

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