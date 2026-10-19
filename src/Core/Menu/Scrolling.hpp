#pragma once

/*
 * Menu list maths, kept separate so it can be tested on a host - see tests/run-tests.sh.
 *
 * option: selected row, index: first visible row, both 1-based. total: rows in the menu being
 * drawn, 0 until it has been drawn once. Invariant kept throughout: the selection is always
 * inside the visible page.
 */

struct MenuSelection
{
   int option = 1;
   int index = 1;
   int total = 0;
};

inline int MaxPageStart(int total, int page)
{
   return (total > page) ? (total - page + 1) : 1;
}

inline bool IsVisible(const MenuSelection& selection, int page)
{
   return (selection.index <= selection.option) && (selection.option < (selection.index + page));
}

/* Move the page to the selection if it is not on it. Never needed from a reachable state. */
inline bool BringIntoView(MenuSelection& selection, int page)
{
   if (IsVisible(selection, page))
      return false;

   if (selection.option < selection.index)
      selection.index = selection.option;
   else
      selection.index = selection.option - page + 1;

   int maxStart = MaxPageStart(selection.total, page);

   if (selection.index < 1)
      selection.index = 1;

   if (selection.index > maxStart)
      selection.index = maxStart;

   return true;
}

inline bool MoveDown(MenuSelection& selection, int page)
{
   /* Nothing drawn yet, so nothing to scroll within. */
   if (selection.total <= 0)
      return false;

   selection.option++;

   if ((selection.option == (selection.index + page - 2)) && (selection.index < (selection.total - page + 1)))
      selection.index++;

   if (selection.option > selection.total)
   {
      selection.option = 1;
      selection.index = 1;
   }

   return BringIntoView(selection, page);
}

inline bool MoveUp(MenuSelection& selection, int page)
{
   if (selection.total <= 0)
      return false;

   selection.option--;

   if ((selection.option == (selection.index + 1)) && (selection.index > 1))
      selection.index--;

   if (selection.option < 1)
   {
      selection.option = selection.total;
      selection.index = (selection.total > page) ? (selection.option - page + 1) : 1;
   }

   return BringIntoView(selection, page);
}

/* The list changed under us - a deleted row, or Restore defaults. */
inline void Clamp(MenuSelection& selection, int page)
{
   if (selection.total <= 0)
      return;

   if (selection.option > selection.total)
      selection.option = selection.total;

   if (selection.option < 1)
      selection.option = 1;

   int maxStart = MaxPageStart(selection.total, page);

   if (selection.index > maxStart)
      selection.index = maxStart;

   if (selection.index < 1)
      selection.index = 1;

   BringIntoView(selection, page);
}