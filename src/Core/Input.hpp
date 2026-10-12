#pragma once
#include <cell/pad/pad_codes.h>
#include "Utils/Timers.hpp"

class Input
{
public:
   enum Buttons
   {
      BUTTON_L2,
      BUTTON_R2,
      BUTTON_L1,
      BUTTON_R1,
      BUTTON_TRIANGLE,
      BUTTON_CIRCLE,
      BUTTON_CROSS,
      BUTTON_SQUARE,
      BUTTON_SELECT,
      BUTTON_L3,
      BUTTON_R3,
      BUTTON_START,
      BUTTON_PAD_UP,
      BUTTON_PAD_RIGHT,
      BUTTON_PAD_DOWN,
      BUTTON_PAD_LEFT,
      BUTTON_PS
   };

   enum Analogs
   {
      ANALOG_RIGHT_X,
      ANALOG_RIGHT_Y,
      ANALOG_LEFT_X,
      ANALOG_LEFT_Y
   };

public:
   explicit Input() = default;
   ~Input() noexcept = default;

   CellPadData* GetInputData();
   void SetInputData(CellPadData* data);
   void ClearInputData();
   bool IsButtonDownFor(Buttons button, uint64_t delay = 0);

   /*
    * Repeat with acceleration, for holding a direction in a list.
    *
    * IsButtonDownFor is a flat rate limiter, and two things about it are wrong for scrolling:
    *
    *   It never resets its timestamp on release. Tapping a direction three times in quick
    *   succession registers once and silently drops the other two, because the second and third
    *   taps arrive inside the delay. That is input loss, not slowness, and it is most of why a list
    *   feels like it is ignoring you.
    *
    *   The rate is constant. Holding a direction should move quickly once you are clearly moving;
    *   a flat rate either crawls for the whole run or is unusable for a single step.
    *
    * This fires on the press itself, waits initialDelay before the first repeat, then shortens the
    * gap by `step` each time until it reaches minDelay and stays there. Releasing resets it, so the
    * next press is immediate again.
    */
   bool IsButtonDownAccelerated(Buttons button, uint64_t initialDelay,
      uint64_t minDelay, uint64_t step);

   /* Whether the button is physically held right now, with no rate limiting at all. */
   bool IsButtonHeld(Buttons button) const;
   // X Axis: -1.00 = Left, 1.00 = Right
   // Y Axis: -1.00 = Top, 1.00 = Bottom
   double GetAnalogInput(Analogs analog);
   double GetRightAnalogX();
   double GetRightAnalogY();
   double GetLeftAnalogX();
   double GetLeftAnalogY();
   // Deadzone is set to 20 by default
   void SetAnalogDeadzone(short deadzone);

private:
   CellPadData m_InputData{};
   uint64_t m_InputButtonLastTime[32]{};
   /* For IsButtonDownAccelerated: how many times each button has repeated since it went down, and
    * whether it was already down last frame. Release detection is what makes the reset work. */
   uint32_t m_InputButtonRepeatCount[32]{};
   bool m_InputButtonWasDown[32]{};
   short m_InputAnalogDeadzone = 20;
};

extern Input g_Input;