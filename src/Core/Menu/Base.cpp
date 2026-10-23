#include "Base.hpp"
#include "../InputUi.hpp"
#include "../Version.hpp"   // QDNSM_VERSION
#include <functional>

Menu g_Menu;

Menu::Menu(Function mainSubmenu)
   : m_MainMenu(mainSubmenu) {}

bool Menu::IsOpened()
{
   return m_OpacityGlobal > 0.0 || m_StateOpened;
}

bool Menu::IsMoving()
{
   return stateMoving;
}

void Menu::SetMovingState(bool state)
{
    stateMoving = state;
}

bool Menu::IsInitialized()
{
   return m_StateInitialized;
}

void Menu::Initialize()
{
   m_StateInitialized = true;
}

bool Menu::IsOpenPressed()
{
	if (g_Input.IsButtonDownFor(Input::BUTTON_R1) && g_Input.IsButtonDownFor(Input::BUTTON_R2) && g_Input.IsButtonDownFor(Input::BUTTON_R3))
   {
      return true;
   }

   return false;
}

bool Menu::IsEnterPressed()
{
   if (g_Input.IsButtonDownFor(Input::BUTTON_CROSS, 400))
   {
      return true;
   }

   return false;
}

bool Menu::IsBackPressed()
{
   if (g_Input.IsButtonDownFor(Input::BUTTON_CIRCLE, 400))
   {
      return true;
   }

   return false;
}

bool Menu::IsEditButtonPressed()
{
	if (g_Input.IsButtonDownFor(Input::BUTTON_SQUARE, 400))
	{
		return true;
	}

	return false;
}

bool Menu::IsDeleteButtonPressed()
{
	if (g_Input.IsButtonDownFor(Input::BUTTON_TRIANGLE, 400))
	{
		return true;
	}

	return false;
}

bool Menu::IsUpPressed()
{
   if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_UP, 150))
   {
      return true;
   }

   return false;
}

bool Menu::IsDownPressed()
{
   if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_DOWN, 150))
   {
      return true;
   }

   return false;
}

bool Menu::IsLeftPressed()
{
   if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_LEFT))
   {
      return true;
   }

   return false;
}

bool Menu::IsRightPressed()
{
   if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_RIGHT, m_SliderDelay))
   {
      return true;
   }

   return false;
}

bool Menu::IsLeftOrRightPressed()
{
   if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_LEFT, m_SliderDelay) || g_Input.IsButtonDownFor(Input::BUTTON_PAD_RIGHT, m_SliderDelay))
   {
      return true;
   }

   return false;
}

bool Menu::IsHovered()
{
   /* An off-screen row must not answer to the buttons. */
   return IsOpened() && IsVisible(m_Selection, maxOptionsPerPage) && (m_Selection.option == m_PrintingOption);
}

bool Menu::IsPressed()
{
   if (IsHovered())
      return IsEnterPressed();

   return false;
}

bool Menu::IsDeletePressed()
{
	if (IsHovered())
		return IsDeleteButtonPressed();

	return false;
}

bool Menu::IsEditPressed()
{
	if (IsHovered())
		return IsEditButtonPressed();

	return false;
}

void Menu::OnOpen()
{
   m_StateOpened = true;
   m_CurrentMenu = (m_SavedMenu == nullptr) ? m_MainMenu : m_SavedMenu;
   m_SubmenuLevel = (m_SavedMenu == nullptr) ? 0 : m_SavedSubmenuLevel;

   if (m_SavedMenu == nullptr)
   {
      m_Selection.option = 1;
      m_Selection.index = 1;
      m_Selection.total = 0;
   }
   else
   {
      m_Selection = m_SavedSelection;
   }

   // opening animation
   g_Timer.Add(&m_OpacityBackground, colorBackground.a, 200);
   g_Timer.Add(&m_OpacityGlobal, 1, 200);
   g_Timer.Add(&m_OpacityHighlightBar, 0.65, 200, 200);
   g_Timer.Add(&m_OpacityText, 1, 200, 200);
   g_Timer.Add(&m_HighlightBarPosY, (m_Selection.option - m_Selection.index) * sizeText, 250);

   stateMoving = true;
}

void Menu::OnClose()
{
   m_StateOpened = false;
   stateMoving = false;
   m_SavedMenu = m_CurrentMenu;
   m_SavedSubmenuLevel = m_SubmenuLevel;
   m_SavedSelection = m_Selection;

   // closing animation
   g_Timer.Add(&m_SizeBackground, 0, 200, 200);
   g_Timer.Add(&m_OpacityBackground, 0, 200, 200);
   g_Timer.Add(&m_OpacityGlobal, 0, 200, 200);
   g_Timer.Add(&m_OpacityHighlightBar, 0, 200);
   g_Timer.Add(&m_OpacityText, 0, 200);
}

void Menu::OnBack()
{
   /* A negative level would index m_LastSubmenu out of bounds. */
   if (m_SubmenuLevel <= 0)
      return;

   m_SubmenuLevel--;
   m_CurrentMenu = m_LastSubmenu[m_SubmenuLevel];
   m_Selection = m_LastSelection[m_SubmenuLevel];

   g_Timer.Add(&m_HighlightBarPosY, (m_Selection.option - m_Selection.index) * sizeText, 250);
}

void Menu::OnScrollUp()
{
   MoveUp(m_Selection, maxOptionsPerPage);
   g_Timer.Add(&m_HighlightBarPosY, (m_Selection.option - m_Selection.index) * sizeText, 250);
}

void Menu::OnScrollDown()
{
   MoveDown(m_Selection, maxOptionsPerPage);
   g_Timer.Add(&m_HighlightBarPosY, (m_Selection.option - m_Selection.index) * sizeText, 250);
}

void Menu::EnterSubmenu(Function submenu)
{
   if (submenu == nullptr)
      return;

   /* m_LastSelection is indexed by the level being left, and the level is about to grow. */
   if ((m_SubmenuLevel + 1) >= static_cast<int>(maxSubmenuLevels))
      return;

   m_LastSubmenu[m_SubmenuLevel] = m_CurrentMenu;
   m_LastSelection[m_SubmenuLevel] = m_Selection;

   m_CurrentMenu = submenu;
   m_Selection.option = 1;
   m_Selection.index = 1;
   m_Selection.total = 0;
   m_SubmenuLevel++;

   g_Timer.Add(&m_HighlightBarPosY, (m_Selection.option - m_Selection.index) * sizeText, 200);
}

void Menu::ChangeSubmenu(Function submenu)
{
   if (submenu != nullptr)
      m_SubmenuDelay = submenu;
}

void Menu::UpdateButtons()
{
    if (!m_StateOpened && (m_OpacityGlobal == 0.0)) // fully closed
    {
        if (IsOpenPressed() &&
            vsh::GetCooperationMode() == vsh::CooperationMode::XMB)
            OnOpen();

        WhileClosed();
    }
    else if (m_StateOpened && (m_OpacityText == 1.0)) // fully opened
    {
        if (IsOpenPressed())
        OnClose();
        else if (IsBackPressed())
        {
        if (m_CurrentMenu == m_MainMenu)
            OnClose();
        else
            OnBack();
        }
        else if (IsUpPressed())
        OnScrollUp();
        else if (IsDownPressed())
        OnScrollDown();

        WhileOpen();
    }
}

void Menu::UpdatePosition()
{
   if (stateMoving)
   {
      double stickX = g_Input.GetRightAnalogX();
      double stickY = -g_Input.GetRightAnalogY();

      if (stickX != 0.0)
      {
         float xAdvance = position.x + stickX * 5.0;
         float xSafeLimit = g_Renderer.m_ViewportWidth / 2 - sizeWidth / 2;

         if (xAdvance > -xSafeLimit && xAdvance < xSafeLimit)
            position.x += stickX * 5.0;

         if (position.x > xSafeLimit)
            position.x = xSafeLimit;

         if (position.x < -xSafeLimit)
            position.x = -xSafeLimit;
      }

      if (stickY != 0.0)
      {
         float yAdvance = position.y + stickY * 5.0;
         float ySafeLimit = g_Renderer.m_ViewportHeight / 2 - m_SizeBackground / 2 - sizeText;

         if (yAdvance > -ySafeLimit && yAdvance < ySafeLimit)
            position.y += stickY * 5.0;

         if (position.y > ySafeLimit)
            position.y = ySafeLimit;

         if (position.y < -ySafeLimit)
            position.y = -ySafeLimit;
      }
   }
}

void Menu::UpdateBackgroundSize()
{
    if (m_StateOpened)
    {
        int optionsCount = (m_Selection.total > maxOptionsPerPage) ? maxOptionsPerPage : m_Selection.total;
        if (!optionsCount)
            return;

        float size = sizeText * optionsCount + m_SizeBackgroundSpace * 2;

        if (!g_Timer.IsAlreadyPresent(&m_SizeBackground))
            g_Timer.Add(&m_SizeBackground, size, 200);

    }
}

void Menu::UpdateHighlightBarPos()
{
    if (m_StateOpened)
    {
        float pos = (m_Selection.option - m_Selection.index) * sizeText;

        if (!g_Timer.IsAlreadyPresent(&m_HighlightBarPosY))
            g_Timer.Add(&m_HighlightBarPosY, pos, 200);
    }
}

void Menu::WhileOpen()
{

}

void Menu::WhileClosed()
{

}

void Menu::UpdateUI()
{
   /*
    * Our own input UI owns the screen while it is active, so the menu is neither
    * drawn nor updated - a half-drawn menu behind a text field looks broken.
    */
   if (g_InputUi.IsActive())
   {
      m_PrintingOption = 0;
      g_InputUi.Update();
      g_InputUi.Draw();
      return;
   }

   if (m_OpacityGlobal > 0.0)
   {
      m_PrintingOption = 0;
      m_DescriptionText.clear();

      /*
       * Keep the selection inside the list when it shrinks underneath us, otherwise deleting
       * the last entry leaves the highlight past the end and nothing is ever hovered again.
       */
      Clamp(m_Selection, maxOptionsPerPage);

      UpdatePosition();
      UpdateBackgroundSize();
      UpdateHighlightBarPos();

      // Update background opacity
      if (m_OpacityGlobal == 1.0)
         m_OpacityBackground = colorBackground.a;

      DrawHeader();
      DrawBody();
      DrawHighlightBar();

      // Display current menu
      if (m_CurrentMenu != nullptr)
         m_CurrentMenu();

      /* From the build that just ran, not the one before it: a stale count wraps to the wrong row. */
      m_Selection.total = m_PrintingOption;

      // Enter selected submenu
      if (m_SubmenuDelay != nullptr)
      {
         EnterSubmenu(m_SubmenuDelay);
         m_SubmenuDelay = nullptr;
      }

      DrawFooter();
      DrawDescription();
   }

}

void Menu::OnUpdate()
{
   if (!IsInitialized())
      Initialize();

   g_Timer.OnUpdate();

   // Close the menu if the xmb isn't loaded
   if (!paf::View::Find("xmb_plugin"))
   {
      if (m_StateOpened)
         OnClose();
      return;
   }

   /*
    * While the input UI owns the screen the menu is invisible and must not react.
    * UpdateButtons() would scroll the hidden list and start tweens on every D-pad
    * press, which is pure waste for something nobody can see.
    */
   if (!g_InputUi.IsActive())
      UpdateButtons();

   UpdateUI();
}

void Menu::DrawHeader()
{
    // Background
    g_Renderer.Rectangle(
        paf::vec2(position.x, position.y + m_SizeBackground / 3.3 + m_SizeBackgroundSpace + sizeText / 2 + sizeHeader / 6.1),
		paf::vec2(sizeWidth, sizeHeader),
        paf::vec4(colorMenu.r, colorMenu.g, colorMenu.b, m_OpacityBackground),
        Renderer::Centered,
        Renderer::Centered);

    // Title
    g_Renderer.Text(
        L"qDNSm",
		paf::vec2(position.x, position.y + m_SizeBackground / 2.54 + sizeTextHeader / 2 + sizeHeader / 7.10),
        sizeTextHeader,
        paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
        Renderer::Centered,
        Renderer::Centered);

	// Version Number
	g_Renderer.Text(
		L"v" QDNSM_VERSION,
		paf::vec2(position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 4, position.y + m_SizeBackground / 2.7 + sizeTextHeader / 2 + sizeHeader / 8.9),
		sizeText / 1.7,
		paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
		Renderer::Right,
		Renderer::Centered);

	// Animated Flame Icon
	g_Renderer.Text(
		L"\uF477",
		paf::vec2(position.x - sizeWidth / 2 - m_SizeBackgroundSpace / 4.78, position.y + m_SizeBackground / 3.15 + sizeTextHeader / 2 + sizeHeader / 2.4),
		sizeText * 1.60,
		paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
		Renderer::Left,
		Renderer::Centered);
}

void Menu::DrawBody()
{
    // Background
    g_Renderer.Rectangle(
        position,
        paf::vec2(sizeWidth, m_SizeBackground),
        paf::vec4(colorBackground.r, colorBackground.g, colorBackground.b, m_OpacityBackground),
        Renderer::Centered,
        Renderer::Centered);
}

void Menu::DrawHighlightBar()
{
    paf::vec2 pos = paf::vec2(position.x, position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - m_HighlightBarPosY - sizeText / 2);

    g_Renderer.Rectangle(
        pos,
        paf::vec2(sizeWidth, sizeText),
        paf::vec4(colorMenu.r, colorMenu.g, colorMenu.b, m_OpacityHighlightBar),
        Renderer::Centered,
        Renderer::Centered);
}

void Menu::DrawFooter()
{
    // Background
    g_Renderer.Rectangle(
        paf::vec2(position.x, position.y - m_SizeBackground / 2 - sizeText / 2),
        paf::vec2(sizeWidth, sizeText),
        paf::vec4(colorMenu.r, colorMenu.g, colorMenu.b, m_OpacityBackground),
        Renderer::Centered,
        Renderer::Centered);

	// Current option counter
	g_Renderer.Text(
		to_wstring(m_Selection.option) + L" / " + to_wstring(m_Selection.total),
		paf::vec2(position.x, position.y - m_SizeBackground / 2 - (sizeText / 1.02) / 2),
		sizeText / 1.2,
		paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
		Renderer::Centered,
		Renderer::Centered);

	// Instructions under the footer
	g_Renderer.Text(
		L"Actions for the selected entry:\n\uF881: Apply | \uF882: Edit | \uF883: Delete",
		paf::vec2(position.x - sizeWidth / 2 + m_SizeBackgroundSpace / 4, position.y - m_SizeBackground / 1.65 - sizeText / 1.8),
		sizeText / 1.14,
		paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
		Renderer::Left,
		Renderer::Top);
}

void Menu::DrawDescription()
{
    if (m_DescriptionText.empty())
        return;

    g_Renderer.Rectangle(
        paf::vec2(position.x, position.y - m_SizeBackground / 2 - sizeTextDescription / 2 - 22.0f),
        paf::vec2(sizeWidth, sizeTextDescription),
        paf::vec4(colorBackground.r, colorBackground.g, colorBackground.b, m_OpacityBackground),
        Renderer::Centered,
        Renderer::Centered);

    g_Renderer.Text(
        m_DescriptionText,
        paf::vec2(
            position.x - sizeWidth / 2 + m_SizeBackgroundSpace / 4,
            position.y - m_SizeBackground / 2 - sizeTextDescription / 2 - 20.0f),
        sizeTextDescription,
        paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
        Renderer::Left,
        Renderer::Centered);
}

void Menu::DrawMenuOption(const std::wstring& text)
{
    if ((m_PrintingOption >= m_Selection.index) && (m_PrintingOption < (m_Selection.index + maxOptionsPerPage)))
    {
        g_Renderer.Text(
            text,
            paf::vec2(position.x - sizeWidth / 2 + m_SizeBackgroundSpace / 2,
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            sizeText,
            paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
            Renderer::Left,
            Renderer::Centered);
    }
}

void Menu::DrawMenuToggle(bool var)
{
    if ((m_PrintingOption >= m_Selection.index) && (m_PrintingOption < (m_Selection.index + maxOptionsPerPage)))
    {
        g_Renderer.Rectangle(
            paf::vec2(position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 2,
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            paf::vec2(sizeText * 0.7, sizeText * 0.7),
            var ? paf::vec4(colorToggleOn.r, colorToggleOn.g, colorToggleOn.b, m_OpacityText)
            : paf::vec4(colorToggleOff.r, colorToggleOff.g, colorToggleOff.b, m_OpacityText),
            Renderer::Right,
            Renderer::Centered,
            "tex_busy");
    }
}

void Menu::DrawMenuSlider(const std::wstring& text, float progress)
{
    if ((m_PrintingOption >= m_Selection.index) && (m_PrintingOption < (m_Selection.index + maxOptionsPerPage)))
    {
        float sliderWidth = sizeWidth / 4;

        // Bar
        g_Renderer.Rectangle(
            paf::vec2(position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 2 - sliderWidth,
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            paf::vec2(sliderWidth, 6),
            paf::vec4(0.15, 0.15, 0.15, m_OpacityText),
            Renderer::Left,
            Renderer::Centered,
            "tex_operation_guide_base");

        // Filled bar
        g_Renderer.Rectangle(
            paf::vec2(position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 2 - sliderWidth,
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            paf::vec2((sliderWidth * progress), 6),
            paf::vec4(colorFillerBar.r, colorFillerBar.g, colorFillerBar.b, m_OpacityText),
            Renderer::Left,
            Renderer::Centered,
            "tex_operation_guide_base");

        // Cursor
        g_Renderer.Rectangle(
            paf::vec2(position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 2 - sliderWidth + (sliderWidth * progress),
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            paf::vec2(sizeText * 0.7, sizeText * 0.7),
            paf::vec4(colorFillerCursor.r, colorFillerCursor.g, colorFillerCursor.b, m_OpacityText),
            Renderer::Centered,
            Renderer::Centered,
            "tex_busy");
    }
}

void Menu::DrawMenuStringOption(const std::wstring& item)
{
    if ((m_PrintingOption >= m_Selection.index) && (m_PrintingOption < (m_Selection.index + maxOptionsPerPage)))
    {
        g_Renderer.Text(
            item,
            paf::vec2(position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 2,
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            sizeText,
            paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
            Renderer::Right,
            Renderer::Centered);
    }
}

Menu& Menu::Option(const std::wstring& text)
{
   m_PrintingOption++;
   DrawMenuOption(text);
   return *this;
}

Menu& Menu::Submenu(Function sub)
{
   if (IsPressed())
      ChangeSubmenu(sub);
   return *this;
}

Menu& Menu::Skip()
{
   if (IsHovered())
   {
      if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_UP))
      {
         OnScrollUp();
      }
      else if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_DOWN))
      {
         OnScrollDown();
      }
   }

   return *this;
}

Menu& Menu::Toggle(bool& var)
{
    DrawMenuToggle(var);
    if (IsPressed())
        var ^= 1;
    return *this;
}

Menu& Menu::Toggle(bool& var, Function onEnable, Function onDisable)
{
   DrawMenuToggle(var);
   if (IsPressed())
   {
      var ^= 1;
      if (var)
      {
         if (onEnable != nullptr)
            onEnable();
      }
      else
      {
         if (onDisable != nullptr)
            onDisable();
      }
   }
   return *this;
}

Menu& Menu::RightText(const std::wstring& text)
{
    if ((m_PrintingOption >= m_Selection.index) && (m_PrintingOption < (m_Selection.index + maxOptionsPerPage)))
    {
        g_Renderer.Text(
            text,
            paf::vec2(
                position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 2,
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            sizeText,
            paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
            Renderer::Right,
            Renderer::Centered);
    }

    return *this;
}

Menu& Menu::Description(const std::wstring& text)
{
   if (IsHovered())
      m_DescriptionText = text;

   return *this;
}

Menu& Menu::Slider(float& var, float min, float max, float step, int decimalPlaces)
{
   processOptionItemControls<float>(var, min, max, step);
   DrawMenuSlider(to_wstring(var, decimalPlaces), (var - min) / (max - min));

   if (IsHovered())
      m_SliderDelay = 20;

   return *this;
}

Menu& Menu::Slider(int& var, int min, int max)
{
   processOptionItemControls<int>(var, min, max, 1);
   DrawMenuSlider(to_wstring(var), static_cast<float>(var - min) / static_cast<float>(max - min));

   if (IsHovered())
      m_SliderDelay = 20;

   return *this;
}

Menu& Menu::Strings(const std::vector<std::wstring>& items, int& index)
{
    int min = 0;
    int max = static_cast<int>(items.size()) - 1;
    processOptionItemControls<int>(index, min, max, 1);

    if (index > static_cast<int>(items.size()) || index < 0)
    {
        g_Renderer.Text(
            L"error (" + to_wstring(index) + L")",
            paf::vec2(
                position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 2,
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            sizeText,
            paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
            Renderer::Right,
            Renderer::Centered);
        return *this;
    }

    if (IsHovered())
    {
        std::wstring selectedItem = items[index];
        std::wstring item = ((index > 0) ? L"\uF886 " : L" ") + selectedItem + ((index < max) ? L" \uF887" : L" ");

        DrawMenuStringOption(item);

        m_SliderDelay = 80;
    }
    else
        DrawMenuStringOption(items[index]);

    return *this;
}
