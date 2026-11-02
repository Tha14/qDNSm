#include "Base.hpp"
#include "../InputUi.hpp"
#include "../Config.hpp"
#include "../MemoryWarning.hpp"
#include "../Version.hpp"   // QDNSM_VERSION
#include "../Language.hpp"
#include <functional>

Menu g_Menu;

static bool CanOpenMenu()
{
   return paf::View::Find("xmb_plugin") != nullptr;
}

static const float UPDATE_NOTICE_PADDING = 8.0f;
static const float UPDATE_NOTICE_GAP = 4.0f;
static const float UPDATE_NOTICE_SPEED = 40.0f;
static const uint64_t UPDATE_NOTICE_STATIC_MS = 5000;
static const uint64_t UPDATE_NOTICE_PAUSE_MS = 5000;
static const uint64_t MEMORY_WARNING_FLASH_MS = 500;
static const uint64_t MEMORY_WARNING_CHECK_MS = 3000;

static float UpdateNoticeGlyphAdvance(wchar_t character, float height)
{
   if ((character == L' ') || (character == L'.') || (character == L':') ||
      (character == L'|') || (character == L'\''))
   {
      return height * 0.30f;
   }

   if ((character == L'i') || (character == L'l') || (character == L'!'))
      return height * 0.28f;

   if (character >= 0x2E80)
      return height * 1.05f;

   return height * 0.62f;
}

static float MeasureUpdateNoticeText(const std::wstring& text, float height)
{
   float width = 0.0f;

   for (size_t i = 0; i < text.size(); i++)
      width += UpdateNoticeGlyphAdvance(text[i], height);

   return width;
}

Menu::Menu(Function mainSubmenu)
   : m_MainMenu(mainSubmenu) {}

bool Menu::IsOpened()
{
   return m_OpacityGlobal > 0.0 || m_StateOpened;
}

bool Menu::IsFullyOpen() const
{
   return m_StateOpened && (m_OpacityText >= 1.0f);
}

void Menu::ForceClose()
{
   g_InputUi.Cancel();
   OnClose();

   g_Timer.Remove(&m_SizeBackground);
   g_Timer.Remove(&m_OpacityBackground);
   g_Timer.Remove(&m_OpacityGlobal);
   g_Timer.Remove(&m_OpacityHighlightBar);
   g_Timer.Remove(&m_OpacityText);

   m_SizeBackground = 0.0f;
   m_OpacityBackground = 0.0f;
   m_OpacityGlobal = 0.0f;
   m_OpacityHighlightBar = 0.0f;
   m_OpacityText = 0.0f;
   m_InputLocked = false;
   m_BusyReconnecting = false;
   m_BusyPatching = false;
   m_SubmenuDelay = nullptr;
   m_DescriptionText.clear();
}

bool Menu::IsMoving()
{
   return stateMoving;
}

void Menu::SetMovingState(bool state)
{
    stateMoving = state;
}

void Menu::SetBusyState(bool busy, bool reconnecting, bool patching)
{
    m_InputLocked = busy;
    m_BusyReconnecting = reconnecting;
    m_BusyPatching = patching;
}

bool Menu::IsInputLocked() const
{
    return m_InputLocked;
}

void Menu::SetOverlay(Function fn)
{
    m_Overlay = fn;
}

void Menu::ClearOverlay()
{
    m_Overlay = nullptr;
}

bool Menu::IsMainMenu() const
{
    return m_CurrentMenu == m_MainMenu;
}

float Menu::GetTextOpacity() const
{
    return m_OpacityText;
}

float Menu::GetBackgroundOpacity() const
{
    return m_OpacityBackground;
}

float Menu::GetBackgroundSize() const
{
    return m_SizeBackground;
}

paf::vec4 Menu::ListTextColor() const
{
    const float alpha = m_OpacityText * (m_InputLocked ? 0.35f : 1.0f);

    if (m_PrintingOptionDisabled)
        return paf::vec4(colorText.r * 0.45f, colorText.g * 0.45f, colorText.b * 0.45f, alpha);

    return paf::vec4(colorText.r, colorText.g, colorText.b, alpha);
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
   return !m_InputLocked && IsOpened() && IsVisible(m_Selection, maxOptionsPerPage) && (m_Selection.option == m_PrintingOption);
}

bool Menu::IsPressed()
{
   if (!m_PrintingOptionDisabled && IsHovered())
      return IsEnterPressed();

   return false;
}

bool Menu::IsDeletePressed()
{
	if (!m_PrintingOptionDisabled && IsHovered())
		return IsDeleteButtonPressed();

	return false;
}

bool Menu::IsEditPressed()
{
	if (!m_PrintingOptionDisabled && IsHovered())
		return IsEditButtonPressed();

	return false;
}

void Menu::OnOpen()
{
   m_StateOpened = true;
   m_StringOptionHeld = false;
   m_StringOptionNextRepeat = 0;
   m_LanguageUnloaded = false;
   Language_LoadAll();
   m_MemoryWarningLevel = GetMemoryWarningLevel();
   m_MemoryWarningNextCheck = GetTimeNow() + MEMORY_WARNING_CHECK_MS;
   m_UpdateNoticeVersion.clear();
   m_UpdateNoticeLanguage.clear();
   m_UpdateNoticeUrl.clear();
   RefreshUpdateNotice();
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
   m_Overlay = nullptr;

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
   m_Overlay = nullptr;

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
   m_Overlay = nullptr;

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
    if (m_InputLocked)
    {
        if (!m_StateOpened && (m_OpacityGlobal == 0.0))
        {
            if (IsOpenPressed() && CanOpenMenu())
                OnOpen();

            WhileClosed();
        }
        else if (m_StateOpened && (m_OpacityText == 1.0))
        {
            if (IsBackPressed())
            {
                if (m_CurrentMenu == m_MainMenu)
                    OnClose();
                else
                    OnBack();
            }
        }

        return;
    }

    if (!m_StateOpened && (m_OpacityGlobal == 0.0)) // fully closed
    {
        if (IsOpenPressed() && CanOpenMenu())
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
   if (m_InputLocked)
      return;

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

         if (!m_UpdateNoticeText.empty())
            ySafeLimit -= (sizeText + 8.0f);

         if (ySafeLimit < 0.0f)
            ySafeLimit = 0.0f;

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
      m_PrintingOptionDisabled = false;
      g_InputUi.Update();
      g_InputUi.Draw();
      return;
   }

   if (!m_StateOpened && (m_OpacityGlobal == 0.0) && !m_InputLocked && !m_LanguageUnloaded)
   {
      Language_Unload();
      m_LanguageUnloaded = true;
   }

   if (m_OpacityGlobal > 0.0)
   {
      m_PrintingOption = 0;
      m_PrintingOptionDisabled = false;
      m_DescriptionText.clear();

      /*
       * Keep the selection inside the list when it shrinks underneath us, otherwise deleting
       * the last entry leaves the highlight past the end and nothing is ever hovered again.
       */
      Clamp(m_Selection, maxOptionsPerPage);

      RefreshUpdateNotice();
      UpdateUpdateNotice();
      UpdatePosition();
      UpdateBackgroundSize();
      UpdateHighlightBarPos();
      UpdateMemoryWarning();

      // Update background opacity
      if (m_OpacityGlobal == 1.0)
         m_OpacityBackground = colorBackground.a;

      DrawHeader();
      DrawUpdateNotice();
      DrawBody();
      DrawMemoryWarning();
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

      if (m_Overlay != nullptr)
         m_Overlay();

      DrawBusyOverlay();
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

void Menu::UpdateMemoryWarning()
{
   const uint64_t now = GetTimeNow();

   if (now < m_MemoryWarningNextCheck)
      return;

   m_MemoryWarningLevel = GetMemoryWarningLevel();
   m_MemoryWarningNextCheck = now + MEMORY_WARNING_CHECK_MS;
}

void Menu::RefreshUpdateNotice()
{
   const std::string& version = g_Config.m_Settings.onlineVersion;
   const std::string& url = g_Config.m_Settings.onlineUrl;
   const std::string& language = Language_ActiveCode();

   if ((version == m_UpdateNoticeVersion) &&
      (url == m_UpdateNoticeUrl) &&
      (language == m_UpdateNoticeLanguage))
   {
      return;
   }

   m_UpdateNoticeVersion = version;
   m_UpdateNoticeUrl = url;
   m_UpdateNoticeLanguage = language;
   m_UpdateNoticeText.clear();
   m_UpdateNoticeTextWidth = 0.0f;
   m_UpdateNoticeMaxOffset = 0.0f;
   m_UpdateNoticeOffset = 0.0f;
   m_UpdateNoticePhase = UpdateNotice_Static;
   m_UpdateNoticePhaseStart = GetTimeNow();

   if (!IsVersionNewer(version, QDNSM_VERSION_ASCII))
      return;

   std::wstring versionWide;
   std::wstring urlWide;
   AsciiToWide(versionWide, version);
   AsciiToWide(urlWide, url);
   m_UpdateNoticeText = FormatText(TEXT_UPDATE_AVAILABLE, versionWide, urlWide);

   const float textHeight = sizeText / 1.2f;
   const float innerWidth = sizeWidth - UPDATE_NOTICE_PADDING * 2.0f;
   m_UpdateNoticeTextWidth = g_Renderer.MeasureText(m_UpdateNoticeText, textHeight);

   if (m_UpdateNoticeTextWidth <= 0.0f)
      m_UpdateNoticeTextWidth = MeasureUpdateNoticeText(m_UpdateNoticeText, textHeight * 0.80f);

   if (m_UpdateNoticeTextWidth > innerWidth)
      m_UpdateNoticeMaxOffset = m_UpdateNoticeTextWidth - innerWidth;
}

void Menu::UpdateUpdateNotice()
{
   if (m_UpdateNoticeText.empty() || (m_UpdateNoticeMaxOffset <= 0.0f))
      return;

   uint64_t now = GetTimeNow();

   if (now < m_UpdateNoticePhaseStart)
      m_UpdateNoticePhaseStart = now;

   const uint64_t elapsed = now - m_UpdateNoticePhaseStart;
   const float distance = (UPDATE_NOTICE_SPEED * static_cast<float>(elapsed)) / 1000.0f;

   switch (m_UpdateNoticePhase)
   {
   case UpdateNotice_Static:
      if (elapsed >= UPDATE_NOTICE_STATIC_MS)
      {
         m_UpdateNoticePhase = UpdateNotice_ScrollLeft;
         m_UpdateNoticePhaseStart = now;
         m_UpdateNoticeOffset = 0.0f;
      }
      break;

   case UpdateNotice_ScrollLeft:
      m_UpdateNoticeOffset = distance;

      if (m_UpdateNoticeOffset >= m_UpdateNoticeMaxOffset)
      {
         m_UpdateNoticeOffset = m_UpdateNoticeMaxOffset;
         m_UpdateNoticePhase = UpdateNotice_PauseLeft;
         m_UpdateNoticePhaseStart = now;
      }
      break;

   case UpdateNotice_PauseLeft:
      if (elapsed >= UPDATE_NOTICE_PAUSE_MS)
      {
         m_UpdateNoticePhase = UpdateNotice_ScrollRight;
         m_UpdateNoticePhaseStart = now;
      }
      break;

   case UpdateNotice_ScrollRight:
      m_UpdateNoticeOffset = m_UpdateNoticeMaxOffset - distance;

      if (m_UpdateNoticeOffset <= 0.0f)
      {
         m_UpdateNoticeOffset = 0.0f;
         m_UpdateNoticePhase = UpdateNotice_PauseRight;
         m_UpdateNoticePhaseStart = now;
      }
      break;

   case UpdateNotice_PauseRight:
      if (elapsed >= UPDATE_NOTICE_PAUSE_MS)
      {
         m_UpdateNoticePhase = UpdateNotice_ScrollLeft;
         m_UpdateNoticePhaseStart = now;
      }
      break;
   }
}

void Menu::DrawUpdateNotice()
{
   if (m_UpdateNoticeText.empty())
      return;

   const float textHeight = sizeText / 1.2f;
   const float innerWidth = sizeWidth - UPDATE_NOTICE_PADDING * 2.0f;

   const float boxHeight = sizeText + 6.0f;
   const float bodyTop = position.y + m_SizeBackground / 2.0f;
   const float headerTop = bodyTop + sizeHeader - 16.0f;
   const float centerY = headerTop + UPDATE_NOTICE_GAP + (boxHeight / 2.0f);

   g_Renderer.Rectangle(
      paf::vec2(position.x, centerY),
      paf::vec2(sizeWidth, boxHeight),
      paf::vec4(colorBackground.r, colorBackground.g, colorBackground.b,
         m_OpacityBackground * 0.88f),
      Renderer::Centered,
      Renderer::Centered);

   g_Renderer.Rectangle(
      paf::vec2(position.x, centerY + (boxHeight / 2.0f) - 1.0f),
      paf::vec2(sizeWidth, 1.5f),
      paf::vec4(colorMenu.r, colorMenu.g, colorMenu.b, m_OpacityBackground * 0.90f),
      Renderer::Centered,
      Renderer::Centered);

   g_Renderer.TextScrolling(
      m_UpdateNoticeText,
      paf::vec2(position.x - (sizeWidth / 2.0f) + UPDATE_NOTICE_PADDING, centerY),
      paf::vec2(innerWidth, boxHeight),
      textHeight,
      m_UpdateNoticeOffset,
      paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
      Renderer::Left,
      Renderer::Centered);
}

void Menu::DrawHeader()
{
	const float bodyTop = position.y + m_SizeBackground / 2.0f;
	const float headerCenterY = bodyTop + sizeHeader / 2.0f - 16.0f;
	const float titleY = headerCenterY + 5.05f;
	const float versionY = headerCenterY + 1.35f;
	const float iconY = headerCenterY + 7.70f;

    // Background
    g_Renderer.Rectangle(
        paf::vec2(position.x, headerCenterY),
		paf::vec2(sizeWidth, sizeHeader),
        paf::vec4(colorMenu.r, colorMenu.g, colorMenu.b, m_OpacityBackground),
        Renderer::Centered,
        Renderer::Centered);

    // Title
    g_Renderer.Text(
        L"qDNSm",
		paf::vec2(position.x, titleY),
        sizeTextHeader,
        paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
        Renderer::Centered,
        Renderer::Centered);

	// Version Number
	g_Renderer.Text(
		L"v" QDNSM_VERSION,
		paf::vec2(position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 4, versionY),
		sizeText / 1.7,
		paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
		Renderer::Right,
		Renderer::Centered);

	// Animated Flame Icon
	g_Renderer.Text(
		L"\uF477",
		paf::vec2(position.x - sizeWidth / 2 - m_SizeBackgroundSpace / 4.78, iconY),
		sizeText * 1.60,
		paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
		Renderer::Left,
		Renderer::Centered);
}

void Menu::DrawMemoryWarning()
{
	if (m_MemoryWarningLevel == MemoryWarningLevel::None)
		return;

	if ((m_MemoryWarningLevel == MemoryWarningLevel::Critical) &&
		((GetTimeNow() / MEMORY_WARNING_FLASH_MS) % 2 != 0))
	{
		return;
	}

	const float headerCenterY = position.y + m_SizeBackground / 2.0f + sizeHeader / 2.0f - 16.0f;
	const float iconY = headerCenterY + 7.70f;
	const float fireX = position.x - sizeWidth / 2.0f - m_SizeBackgroundSpace / 4.78f;
	const float markX = fireX + sizeText * 1.75f;
	const float markWidth = sizeText * 0.20f;
	const float stemHeight = sizeText * 0.75f;
	const float dotHeight = sizeText * 0.28f;
	const float gap = sizeText * 0.08f;
	const float totalHeight = stemHeight + gap + dotHeight;
	const float markBottom = iconY - totalHeight / 2.0f;
	const float dotCenter = markBottom + dotHeight / 2.0f;
	const float stemCenter = markBottom + dotHeight + gap + stemHeight / 2.0f;
	const paf::vec4 color(1.0f, 1.0f, 0.0f, m_OpacityText);

	g_Renderer.Rectangle(
		paf::vec2(markX, stemCenter),
		paf::vec2(markWidth, stemHeight),
		color,
		Renderer::Left,
		Renderer::Centered);

	g_Renderer.Rectangle(
		paf::vec2(markX, dotCenter),
		paf::vec2(markWidth, dotHeight),
		color,
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
	std::wstring instructions;

	if (m_InputLocked)
	{
		instructions = T(TEXT_FOOTER_WAIT_CLOSE);
	}
	else if (m_CurrentMenu == m_MainMenu)
	{
		instructions = T(TEXT_FOOTER_ACTIONS);
		instructions += L"\n";
		instructions += T(g_Config.m_Settings.showSupportedGames
			? TEXT_FOOTER_MAIN_HIDE
			: TEXT_FOOTER_MAIN_SHOW);
	}
	else
	{
		return;
	}

	g_Renderer.Text(
		instructions,
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

void Menu::DrawBusyOverlay()
{
    if (!m_InputLocked || !m_StateOpened || (m_SizeBackground < 56.0f))
        return;

    const wchar_t* dots[] = { L"", L".", L"..", L"..." };
    int dot = static_cast<int>((GetTimeNow() / 350) % 4);
    std::wstring wait = T(TEXT_BUSY_PLEASE_WAIT);
    wait += dots[dot];

    g_Renderer.Rectangle(
        position,
        paf::vec2(sizeWidth, m_SizeBackground),
        paf::vec4(0.0f, 0.0f, 0.0f, 0.65f * m_OpacityText),
        Renderer::Centered,
        Renderer::Centered);

    g_Renderer.RectangleOnTop(
        position,
        paf::vec2(sizeWidth - 20.0f, 56.0f),
        paf::vec4(0.0f, 0.0f, 0.0f, 0.80f * m_OpacityText),
        Renderer::Centered,
        Renderer::Centered);

    g_Renderer.Rectangle(
        paf::vec2(position.x, position.y + 27.0f),
        paf::vec2(sizeWidth - 20.0f, 2.0f),
        paf::vec4(colorMenu.r, colorMenu.g, colorMenu.b, 0.90f * m_OpacityText),
        Renderer::Centered,
        Renderer::Centered);

    TextId phase = TEXT_BUSY_APPLYING;

    if (m_BusyReconnecting)
        phase = TEXT_BUSY_RECONNECTING;
    else if (m_BusyPatching)
        phase = TEXT_BUSY_OPENSPY_PATCH;

    g_Renderer.TextOnTop(
        T(phase),
        paf::vec2(position.x, position.y + 9.0f),
        sizeText / 1.15f,
        paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
        Renderer::Centered,
        Renderer::Centered);

    g_Renderer.TextOnTop(
        wait,
        paf::vec2(position.x, position.y - 10.0f),
        sizeText / 1.45f,
        paf::vec4(colorText.r, colorText.g, colorText.b, m_OpacityText),
        Renderer::Centered,
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
            ListTextColor(),
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
            ListTextColor(),
            Renderer::Right,
            Renderer::Centered);
    }
}

Menu& Menu::Option(const std::wstring& text, bool disabled)
{
   m_PrintingOption++;
   m_PrintingOptionDisabled = disabled;
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
            ListTextColor(),
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
    const uint64_t repeatDelay = 250;
    const bool leftHeld = g_Input.IsButtonHeld(Input::BUTTON_PAD_LEFT);
    const bool rightHeld = g_Input.IsButtonHeld(Input::BUTTON_PAD_RIGHT);

    if (!leftHeld && !rightHeld)
    {
        m_StringOptionHeld = false;
        m_StringOptionNextRepeat = 0;
    }
    else if (IsHovered())
    {
        const uint64_t timeNow = GetTimeNow();
        bool step = false;

        if (!m_StringOptionHeld)
        {
            m_StringOptionHeld = true;
            m_StringOptionNextRepeat = timeNow + repeatDelay;
            step = true;
        }
        else if (timeNow >= m_StringOptionNextRepeat)
        {
            m_StringOptionNextRepeat = timeNow + repeatDelay;
            step = true;
        }

        if (step)
        {
            if (leftHeld && !rightHeld && (index > min))
                index--;
            else if (rightHeld && !leftHeld && (index < max))
                index++;
        }
    }
    else
    {
        m_StringOptionHeld = false;
        m_StringOptionNextRepeat = 0;
    }

    if (index > static_cast<int>(items.size()) || index < 0)
    {
        g_Renderer.Text(
            FormatText(TEXT_MENU_INDEX_ERROR, to_wstring(index)),
            paf::vec2(
                position.x + sizeWidth / 2 - m_SizeBackgroundSpace / 2,
                position.y + m_SizeBackground / 2 - m_SizeBackgroundSpace - (m_PrintingOption - m_Selection.index) * sizeText - sizeText / 2),
            sizeText,
            ListTextColor(),
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
