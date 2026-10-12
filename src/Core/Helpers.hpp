#pragma once
#include <string>
#include <vsh/plugins.hpp>
#include <vsh/paf.hpp>
#include <vsh/vshcommon.hpp>
#include <vsh/vshmain.hpp>
#include "Utils/SystemCalls.hpp"
#include "Utils/Timers.hpp"
#include "Utils/FileSystem.hpp"
#include "Core/Input.hpp"

class Helpers
{
public:
   Helpers();

   void OnUpdate();

public:
   paf::View* m_SystemPlugin{};
   paf::View* m_XmbPlugin{};
   paf::PhWidget* m_PageAutoOffGuide{};

   bool m_IsHen{};
};

extern Helpers g_Helpers;