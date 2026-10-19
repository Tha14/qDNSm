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
};

extern Helpers g_Helpers;