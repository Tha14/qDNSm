#include "Input.hpp"

Input g_Input;

CellPadData* Input::GetInputData()
{
   return &m_InputData;
}

void Input::SetInputData(CellPadData* data)
{
   vsh::memcpy(&m_InputData, data, sizeof(CellPadData));
}

void Input::ClearInputData()
{
   vsh::memset(&m_InputData, 0, sizeof(CellPadData));
   m_InputData.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] =
      m_InputData.button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] =
      m_InputData.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X] =
      m_InputData.button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y] = 128;
}

bool Input::IsButtonHeld(Buttons button) const
{
   const uint32_t buttonData = ((m_InputData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] << 8) | m_InputData.button[CELL_PAD_BTN_OFFSET_DIGITAL2]);

   return (buttonData & (1 << button)) != 0;
}

bool Input::IsButtonDownAccelerated(Buttons button, uint64_t initialDelay,
   uint64_t minDelay, uint64_t step)
{
   const uint64_t timeNow = GetTimeNow();

   if (!IsButtonHeld(button))
   {
      /*
       * Release resets everything. This is the part IsButtonDownFor is missing, and it is what
       * stops quick successive taps from being swallowed: without it, a tap that arrives inside
       * the repeat delay is indistinguishable from the button still being held, so it is dropped.
       */
      m_InputButtonRepeatCount[button] = 0;
      m_InputButtonWasDown[button] = false;
      m_InputButtonLastTime[button] = timeNow;

      return false;
   }

   /* The frame the button is first seen down is the press itself - always fires, never delayed. */
   if (!m_InputButtonWasDown[button])
   {
      m_InputButtonWasDown[button] = true;
      m_InputButtonLastTime[button] = timeNow;
      m_InputButtonRepeatCount[button] = 1;

      return true;
   }

   const uint32_t repeats = m_InputButtonRepeatCount[button];

   /* The first repeat after the press waits the full initial delay; later ones shorten. */
   uint64_t threshold = initialDelay;

   if (repeats > 1)
   {
      const uint64_t eased = (step * (repeats - 1) < initialDelay)
         ? (initialDelay - (step * (repeats - 1)))
         : 0;

      threshold = (eased < minDelay) ? minDelay : eased;
   }

   if ((timeNow - m_InputButtonLastTime[button]) >= threshold)
   {
      m_InputButtonLastTime[button] = timeNow;
      m_InputButtonRepeatCount[button] = repeats + 1;

      return true;
   }

   return false;
}

bool Input::IsButtonDownFor(Buttons button, uint64_t delay)
{
   uint64_t timeNow = GetTimeNow();
   if (timeNow - m_InputButtonLastTime[button] > delay)
   {
      uint32_t buttonData = ((m_InputData.button[CELL_PAD_BTN_OFFSET_DIGITAL1] << 8) | m_InputData.button[CELL_PAD_BTN_OFFSET_DIGITAL2]);
      if (buttonData & (1 << button))
      {
         m_InputButtonLastTime[button] = timeNow;
         return true;
      }
   }
   return false;
}

double Input::GetAnalogInput(Analogs analog)
{
   int16_t input = m_InputData.button[4 + analog] - 128;
   if ((input > -m_InputAnalogDeadzone) && (input < m_InputAnalogDeadzone))
      return 0.0;

   return static_cast<double>(input) / 128.0;
}

double Input::GetRightAnalogX()
{
   return GetAnalogInput(ANALOG_RIGHT_X);
}

double Input::GetRightAnalogY()
{
   return GetAnalogInput(ANALOG_RIGHT_Y);
}

double Input::GetLeftAnalogX()
{
   return GetAnalogInput(ANALOG_LEFT_X);
}

double Input::GetLeftAnalogY()
{
   return GetAnalogInput(ANALOG_LEFT_Y);
}

void Input::SetAnalogDeadzone(short deadzone)
{
   m_InputAnalogDeadzone = deadzone;
}