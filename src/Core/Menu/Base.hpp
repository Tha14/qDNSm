#pragma once

#include <vsh/paf.hpp>
#include <vsh/vshcommon.hpp>
#include "Utils/Timers.hpp"
#include "Utils/Std.hpp"
#include "Core/Input.hpp"
#include "Core/Renderer.hpp"
#include "Core/Helpers.hpp"
#include "Scrolling.hpp"
#include <functional>

using Function = void(*)();

class Menu
{
public:
   Menu() = default;
   Menu(Function mainSubmenu);

   bool IsOpened();
   bool IsMoving();
   void SetMovingState(bool state);
   void OnUpdate();

   Menu& Option(const std::wstring& text);
   Menu& Submenu(Function sub);
   Menu& Skip();
   Menu& Toggle(bool& var);
   Menu& Toggle(bool& var, Function onEnable, Function onDisable);

   /*
    * The menu is immediate-mode: these callbacks are invoked during the build pass and are
    * never stored, so they are templated on the callable. That gives lambdas full capture
    * support with no type erasure. std::function is not used because this toolchain's libc++
    * does not provide it.
    */
   template <typename Callback>
   Menu& Action(Callback fn)
   {
      if (IsPressed())
         fn();

      return *this;
   }

   template <typename Callback>
   Menu& DeleteAction(Callback fn)
   {
      if (IsDeletePressed())
         fn();

      return *this;
   }

   template <typename Callback>
   Menu& EditAction(Callback fn)
   {
      if (IsEditPressed())
         fn();

      return *this;
   }

   template <typename Callback>
   Menu& WhileHovered(Callback fn)
   {
      if (IsHovered())
         fn();

      return *this;
   }

   Menu& RightText(const std::wstring& text);
   Menu& Description(const std::wstring& text);
   Menu& Slider(float& var, float min, float max, float step, int decimalPlaces);
   Menu& Slider(int& var, int min, int max);
   Menu& Strings(const std::vector<std::wstring>& display, int& index);

private:
   bool IsInitialized();
   void Initialize();

   bool IsOpenPressed();
   bool IsEnterPressed();
   bool IsEditPressed();
   bool IsEditButtonPressed();
   bool IsDeleteButtonPressed();
   bool IsBackPressed();
   bool IsUpPressed();
   bool IsDownPressed();
   bool IsLeftPressed();
   bool IsRightPressed();
   bool IsLeftOrRightPressed();
   bool IsHovered();
   bool IsPressed();
   bool IsDeletePressed();

   void OnOpen();
   void OnClose();
   void OnBack();
   void OnScrollUp();
   void OnScrollDown();

   void EnterSubmenu(Function submenu);
   void ChangeSubmenu(Function submenu);

   void WhileOpen();
   void WhileClosed();

   void UpdateButtons();
   void UpdatePosition();
   void UpdateBackgroundSize();
   void UpdateHighlightBarPos();
   void UpdateUI();

   void DrawHeader();
   void DrawBody();
   void DrawHighlightBar();
   void DrawFooter();
   void DrawDescription();

   void DrawMenuOption(const std::wstring& text);
   void DrawMenuToggle(bool var);
   void DrawMenuSlider(const std::wstring& text, float progress);
   void DrawMenuStringOption(const std::wstring& item);

   template <typename T>
   void processOptionItemControls(T& var, T min, T max, T step)
   {
      if (IsHovered())
      {
         if (IsLeftPressed())
         {
            if (var <= min)
               var = min;
            else
               var -= step;
         }

         if (var < min)
            var = min;

         if (IsRightPressed())
         {
            if (var >= max)
               var = max;
            else
               var += step;
         }

         if (var > max)
            var = max;
      }
   }

public:
   bool stateMoving{};
   paf::vec2 position{ -480.0, -35.0 };
   paf::vec2 safeArea{ 31.0, 18.0 };
   float sizeHeader = 40.0;
   float sizeWidth = 214.0;
   float sizeText = 18.0;
   float sizeTextHeader = 30.0;
   float sizeTextDescription = 18.0;
   paf::vec4 colorMenu{ 1.0, 0.0, 0.0, 0.80 };
   paf::vec4 colorBackground{ 0, 0, 0, 0.914 };
   paf::vec4 colorText{ 1, 1, 1, 1 };
   paf::vec4 colorFillerBar{ 0.0, 1.0, 1.0, 0.9 };
   paf::vec4 colorFillerCursor{ 0.423, 0.443, 0.415, 0.9 };
   paf::vec4 colorToggleOn{ 0.0, 1.0, 0.0, 1.0 };
   paf::vec4 colorToggleOff{ 1.0, 0.0, 0.0, 1.0 };

private:
   bool m_StateInitialized{};
   bool m_StateOpened{};

   Function m_OnMain{};
   Function m_MainMenu{};
   Function m_CurrentMenu{};
   Function m_SavedMenu{};
   Function m_SubmenuDelay{};

   static const unsigned maxSubmenuLevels = 20;
   static const unsigned maxOptionsPerPage = 5;
   Function m_LastSubmenu[maxSubmenuLevels]{};
   MenuSelection m_LastSelection[maxSubmenuLevels]{};

   int m_SavedSubmenuLevel = 0;
   MenuSelection m_SavedSelection{};

   int m_SubmenuLevel = 0;
   int m_PrintingOption = 0;

   /* Carries its own row count: it used to be one global read from the previous frame, which
    * after a submenu switch described the wrong menu. */
   MenuSelection m_Selection{};

   float m_OpacityGlobal = 0;
   float m_OpacityBackground = 0;
   float m_OpacityHighlightBar = 0;
   float m_OpacityText = 0;

   float m_SizeBackground = 0;
   float m_SizeBackgroundSpace = 10;

   float m_HighlightBarPosY = 0;

   uint64_t m_SliderDelay = 60;

   std::wstring m_DescriptionText;

   bool m_EditingAlpha = true;
   paf::vec4* m_ColorToEdit{};
   Function m_ColorChangeFn{};
};

extern Menu g_Menu;