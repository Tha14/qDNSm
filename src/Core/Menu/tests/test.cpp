/*
 * Menu list maths, plus the frame ordering it depends on. Scrolling.hpp is included as it
 * ships. Usage: sh src/Core/Menu/tests/run-tests.sh
 */

#include "../Scrolling.hpp"

#include <cstdio>

static const int PAGE = 5;

static int g_Failures = 0;

static void Check(bool condition, const char* what)
{
   if (condition)
      return;

   std::printf("  FAIL: %s\n", what);
   g_Failures++;
}

static void CheckEq(int actual, int expected, const char* what)
{
   if (actual == expected)
      return;

   std::printf("  FAIL: %s (expected %d, got %d)\n", what, expected, actual);
   g_Failures++;
}

static void CheckInvariant(const MenuSelection& selection, int page, const char* what)
{
   Check(selection.total > 0, what);
   Check(selection.option >= 1, what);
   Check(selection.option <= selection.total, what);
   Check(selection.index >= 1, what);
   Check(selection.index <= MaxPageStart(selection.total, page), what);
   Check(IsVisible(selection, page), what);
}

static void WalkToBottom(MenuSelection& selection, int page)
{
   while (selection.option < selection.total)
      MoveDown(selection, page);
}

/* Menu::UpdateUI, so the ordering the fix is about is tested rather than assumed. */
struct Frame
{
   int rows = 0;
   int printing = 0;
   int level = 0;
   bool inSubmenu = false;

   MenuSelection selection{};
   MenuSelection last[4]{};

   void EnterSubmenu()
   {
      last[level] = selection;
      inSubmenu = true;
      level++;

      selection.option = 1;
      selection.index = 1;
      selection.total = 0;
   }

   void Back()
   {
      level--;
      inSubmenu = false;
      selection = last[level];
   }

   void Run()
   {
      printing = rows; /* the m_CurrentMenu() pass */
      Clamp(selection, PAGE);
      selection.total = printing;
   }
};

/* The loop as it was before the fix. Kept so the regression stays documented: one global row
 * count read from the previous frame, and OnBack restoring the position without the count. */
struct LegacyFrame
{
   int rows = 0;
   int printing = 0;
   int level = 0;
   bool inSubmenu = false;

   int option = 1;
   int index = 1;
   int total = 0;

   int lastOption[4]{};
   int lastIndex[4]{};

   void EnterSubmenu()
   {
      lastOption[level] = option;
      lastIndex[level] = index;
      inSubmenu = true;
      level++;

      option = 1;
      index = 1;
   }

   void Back()
   {
      level--;
      inSubmenu = false;
      option = lastOption[level];
      index = lastIndex[level];
   }

   void Run()
   {
      total = printing;
      if ((total > 0) && (option > total))
      {
         option = total;
         if (index > option)
            index = option;
      }

      printing = rows;
   }
};

/* Leaving a submenu puts the highlight back on the row that opened it. */
static void TestBackOutOfSubmenuRestoresRow()
{
   Frame frame;
   frame.rows = 16;
   frame.Run();

   WalkToBottom(frame.selection, PAGE);
   CheckEq(frame.selection.option, 16, "walked to the last row of the main list");
   CheckEq(frame.selection.index, 12, "which is on the last page");

   frame.EnterSubmenu();

   frame.rows = 5;
   frame.Run(); /* the submenu is drawn */

   frame.Back();

   frame.rows = 16;
   frame.Run();

   CheckEq(frame.selection.option, 16, "back out returns to the row that opened the submenu");
   CheckEq(frame.selection.index, 12, "back out returns to the page it was on");
   CheckEq(frame.selection.total, 16, "back out measures the list against itself, not the submenu");
}

/* The same flow when the menu was closed while inside the submenu and then reopened. */
static void TestBackOutAfterCloseInsideSubmenu()
{
   Frame frame;
   frame.rows = 16;
   frame.Run();
   WalkToBottom(frame.selection, PAGE);

   MenuSelection savedOnClose = frame.selection;

   frame.EnterSubmenu();
   frame.rows = 5;
   frame.Run();

   frame.selection = savedOnClose; /* OnClose then OnOpen */
   frame.inSubmenu = true;
   frame.Run();

   CheckEq(frame.selection.total, 5, "reopened inside the submenu, counted as the submenu");

   frame.Back();
   frame.rows = 16;
   frame.Run();

   CheckEq(frame.selection.option, 16, "back out after a reopen still returns to row 16");
   CheckEq(frame.selection.total, 16, "and to the main list's own row count");
}

/* What the old ordering did: any list longer than the submenu landed on row 5. */
static void TestLegacyOrderingLostTheRow()
{
   for (int rows = 6; rows <= 20; rows++)
   {
      LegacyFrame frame;
      frame.rows = rows;
      frame.Run();

      while (frame.option < rows)
      {
         frame.option++;
         if (frame.option > rows)
         {
            frame.option = 1;
            frame.index = 1;
         }
      }

      frame.EnterSubmenu();
      frame.rows = 5;
      frame.Run();

      frame.Back();
      frame.rows = rows;
      frame.Run();

      CheckEq(frame.option, 5, "the old ordering dropped the selection to the submenu's row count");
   }
}

/* Short lists were never broken, and must not start being. */
static void TestShortListsUnaffected()
{
   for (int rows = 1; rows <= PAGE; rows++)
   {
      MenuSelection selection;
      selection.total = rows;
      WalkToBottom(selection, PAGE);

      CheckEq(selection.option, rows, "a list that fits on one page walks to its last row");
      CheckEq(selection.index, 1, "and stays on the first page");

      MoveUp(selection, PAGE);
      CheckInvariant(selection, PAGE, "short list stays valid across a wrap");
   }
}

/* Pressing up at the top has to land on the bottom row of the bottom page. */
static void TestWrapFromTop()
{
   for (int rows = 1; rows <= 40; rows++)
   {
      MenuSelection selection;
      selection.total = rows;
      WalkToBottom(selection, PAGE);

      while (selection.option != 1)
         MoveUp(selection, PAGE);

      MoveUp(selection, PAGE);

      CheckEq(selection.option, rows, "up from the top wraps to the last row");
      CheckEq(selection.index, MaxPageStart(rows, PAGE), "and shows the last page");
      CheckInvariant(selection, PAGE, "wrap from the top stays valid");
   }
}

/* Pressing down at the bottom has to land back on the top row. */
static void TestWrapFromBottom()
{
   for (int rows = 1; rows <= 40; rows++)
   {
      MenuSelection selection;
      selection.total = rows;
      WalkToBottom(selection, PAGE);

      MoveDown(selection, PAGE);

      CheckEq(selection.option, 1, "down from the bottom wraps to the first row");
      CheckEq(selection.index, 1, "and shows the first page");
      CheckInvariant(selection, PAGE, "wrap from the bottom stays valid");
   }
}

/* The cursor must never leave the visible page. */
static void TestExhaustiveSweep()
{
   for (int rows = 1; rows <= 40; rows++)
   {
      for (int start = 1; start <= rows; start++)
      {
         for (int direction = 0; direction < 2; direction++)
         {
            MenuSelection selection;
            selection.total = rows;
            selection.option = start;
            selection.index = start;
            Clamp(selection, PAGE);

            for (int step = 0; step < (rows * 4) + 3; step++)
            {
               if (direction == 0)
                  MoveDown(selection, PAGE);
               else
                  MoveUp(selection, PAGE);

               CheckInvariant(selection, PAGE, "swept selection stays valid");
            }
         }
      }
   }
}

/* Deleting the entry under the cursor, and Restore defaults rewriting the whole list. */
static void TestListShrinksUnderTheCursor()
{
   MenuSelection selection;
   selection.total = 16;
   WalkToBottom(selection, PAGE);

   while (selection.total > 3)
   {
      selection.total--;
      Clamp(selection, PAGE);
      CheckInvariant(selection, PAGE, "survives the list shrinking under it");
   }

   CheckEq(selection.option, 3, "the cursor ends on the last remaining row");

   selection.total = 5;
   selection.option = 21;
   selection.index = 17;
   Clamp(selection, PAGE);

   CheckEq(selection.option, 5, "a rewrite pulls the cursor back into the new list");
   CheckInvariant(selection, PAGE, "and leaves it on a real page");
}

/* An unknown row count means the menu has not been drawn yet. */
static void TestUnknownTotalIsInert()
{
   MenuSelection selection;

   MoveDown(selection, PAGE);
   MoveUp(selection, PAGE);
   Clamp(selection, PAGE);

   CheckEq(selection.option, 1, "a move against an unknown count changes nothing");
   CheckEq(selection.index, 1, "including the page");

   /* A submenu's first frame. Must not wrap against the count of the menu above it. */
   selection.total = 16;
   selection.option = 1;
   selection.index = 1;
   selection.total = 0;

   MoveUp(selection, PAGE);
   CheckEq(selection.option, 1, "and it stays put rather than wrapping to a foreign count");
}

/* The page creep has to survive: it never looks like the cursor stopped at an edge. */
static void TestInfiniteScrollFeelIsPreserved()
{
   MenuSelection selection;
   selection.total = 16;

   MoveDown(selection, PAGE);
   CheckEq(selection.option, 2, "first press down moves one row");
   CheckEq(selection.index, 1, "page has not moved yet");

   MoveDown(selection, PAGE);
   CheckEq(selection.option, 3, "second press down");
   CheckEq(selection.index, 1, "page still has not moved");

   MoveDown(selection, PAGE);
   CheckEq(selection.option, 4, "third press down");
   CheckEq(selection.index, 2, "page follows the cursor one row on");
   CheckEq(selection.option - selection.index, 2, "cursor stays the third visible row");

   MoveDown(selection, PAGE);
   CheckEq(selection.index, 3, "and it keeps following, one row per press");
   CheckEq(selection.option - selection.index, 2, "cursor still the third visible row");

   MoveUp(selection, PAGE);
   CheckEq(selection.index, 2, "going back up reverses it");
   CheckEq(selection.option - selection.index, 2, "cursor still the third visible row");

   MoveUp(selection, PAGE);
   CheckEq(selection.index, 1, "and back to the first page");
   CheckEq(selection.option - selection.index, 2, "cursor still the third visible row");
}

/* BringIntoView must never fire in real use. If this fails, the feel has changed. */
static void TestRepairNeverFiresFromReachableStates()
{
   for (int rows = 1; rows <= 40; rows++)
   {
      for (int direction = 0; direction < 2; direction++)
      {
         MenuSelection selection;
         selection.total = rows;

         for (int step = 0; step < (rows * 4) + 3; step++)
         {
            if (direction == 0)
               Check(!MoveDown(selection, PAGE), "scrolling down never needs the safety net");
            else
               Check(!MoveUp(selection, PAGE), "scrolling up never needs the safety net");
         }
      }
   }
}

int main()
{
   std::printf("back out of submenu restores row\n");
   TestBackOutOfSubmenuRestoresRow();

   std::printf("back out after a reopen\n");
   TestBackOutAfterCloseInsideSubmenu();

   std::printf("legacy ordering lost the row\n");
   TestLegacyOrderingLostTheRow();

   std::printf("short lists\n");
   TestShortListsUnaffected();

   std::printf("wrap from top\n");
   TestWrapFromTop();

   std::printf("wrap from bottom\n");
   TestWrapFromBottom();

   std::printf("exhaustive sweep\n");
   TestExhaustiveSweep();

   std::printf("list shrinks under the cursor\n");
   TestListShrinksUnderTheCursor();

   std::printf("unknown row count\n");
   TestUnknownTotalIsInert();

   std::printf("infinite scroll feel preserved\n");
   TestInfiniteScrollFeelIsPreserved();

   std::printf("safety net never fires in real use\n");
   TestRepairNeverFiresFromReachableStates();

   if (g_Failures != 0)
   {
      std::printf("\n%d check(s) failed\n", g_Failures);
      return 1;
   }

   std::printf("\nall checks passed\n");
   return 0;
}