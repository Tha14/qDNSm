#include "Helpers.hpp"

Helpers g_Helpers;

Helpers::Helpers()
{
    m_IsHen = IsConsoleHen();
}

void Helpers::OnUpdate()
{
    // Update plugins and widgets pointers
    m_SystemPlugin = paf::View::Find("system_plugin");

    m_PageAutoOffGuide = m_SystemPlugin ? m_SystemPlugin->FindWidget("page_autooff_guide") : nullptr;

    //if (g_Input.IsButtonBinds(Input::BUTTON_L1, Input::BUTTON_PAD_UP)) TakeScreenshot();
}