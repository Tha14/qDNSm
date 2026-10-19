#pragma once
#include <string>
#include <vsh/paf.hpp>
#include "Renderer.hpp"

/*
 * A self-contained on-screen input UI: free text, a 10-key numeric pad, and a Yes/No
 * confirm. Written because the PS3 sysutil dialogs do not work from this module:
 * cellMsgDialogOpen2 never renders, and cellOskDialogLoadAsync needs a memory container
 * that this process cannot create (CELL_ENOMEM).
 *
 * It runs as a mode over frames on the XMB thread - no blocking, no worker thread - which
 * is why it does not have the deadlock problem that killed the earlier attempts.
 */
class InputUi
{
public:
	enum Mode
	{
		Mode_None,
		Mode_Text,
		Mode_Numeric,
		Mode_Confirm,
		/*
		 * Read-only multi-line text, dismissed with any button. Used for the How-to-use and About
		 * screens, which are instructions rather than questions - Mode_Confirm draws a Yes/No pair,
		 * which is a lie when there is nothing to decide.
		 */
		Mode_Info
	};

	enum Result
	{
		Result_None,
		Result_Confirmed,
		Result_Cancelled,

		/*
		 * A read-only screen was dismissed. Not a step in anything.
		 *
		 * It exists because reporting Result_Confirmed for these screens is actively harmful:
		 * UpdateDnsFlow() treats a confirmed result as "a flow step finished", and an info screen
		 * finishes no step, so the response belonged to whatever flow was in progress. Closing
		 * either info screen opened the "Primary DNS IP address" prompt, because the Add/Edit
		 * branch is reached by any confirmed result with no matching flow.
		 *
		 * This result must never be read as an answer to a question that was not asked.
		 */
		Result_Info
	};

public:
	InputUi() = default;

	void BeginText(const std::wstring& title, const std::string& initial, bool numeric);
	void BeginConfirm(const std::wstring& title, const std::string& message);

	/*
	 * Read-only text screen. Newlines in the message start new lines; a blank line is drawn as a
	 * paragraph break with extra spacing.
	 *
	 * An optional credit block is drawn along the bottom of the card, centred: a small dim label
	 * above a larger accent-coloured name. Both are optional and independent - a screen can want the
	 * name without a label, though there is no reason to yet.
	 *
	 * The label lives here rather than as the last line of the body because the body is drawn left
	 * aligned and starts at a fixed offset from the top; a line meaning to sit above a centred name
	 * needs to be positioned against it, not against the paragraph above.
	 *
	 * Lines are clipped, not wrapped - see the implementation.
	 */
	void BeginInfo(const std::wstring& title, const std::string& message,
		const std::wstring& creditLabel = L"", const std::wstring& credit = L"");


	/* Reads input for this frame. Call first, then Draw(). */
	void Update();

	void Draw();

	bool IsActive() const { return m_Mode != Mode_None; }

	/* Set for one frame after a mode finishes, until consumed. */
	bool HasResult() const { return m_HasResult; }
	Result GetResult() const { return m_Result; }
	const std::string& GetText() const { return m_Text; }
	void ConsumeResult();

private:
	void AppendCharacter(char character);
	void Backspace();
	bool ConfirmButtonPressed();
	bool CancelButtonPressed();
	bool AcceptButtonPressed();
	void DrawBox(float width, float height);
	void DrawTitle();
	void DrawValue();
	void DrawFooter(const wchar_t* left, const wchar_t* right);
	void DrawSelection(float centerX, float centerY, float w, float h);
	void DrawCharacterGrid();
	void DrawNumericPad();
	void DrawConfirm();
	void DrawInfo();

	Mode m_Mode{};
	Result m_Result{ Result_None };
	bool m_HasResult{};

	std::wstring m_Title{};
	std::string m_Text{};
	std::wstring m_Message{};

	/* Optional credit block at the foot of a Mode_Info card: a small label and a larger name, both
	 * centred and both empty by default. Reset by every other Begin*, so a leftover cannot appear on
	 * a screen that never asked for one. */
	std::wstring m_CreditLabel{};
	std::wstring m_Credit{};

	int m_Column{};
	int m_Row{};
	bool m_ConfirmYes{};

	/* Sticky letter case for the text grid. The layout is authored in uppercase, so
	 * lowercase simply folds letters on the way in. */
	bool m_UpperCase{ true };

	/*
	 * The renderer's origin is the screen centre, not the top-left, AND y grows UPWARD:
	 * the menu's y decreases as the selection moves down the list. So the visual top of
	 * the box is the LARGER y.
	 */
	float m_BoxLeft{};
	float m_BoxMinY{};   /* visual bottom */
	float m_BoxMaxY{};   /* visual top */
	float m_BoxWidth{};
	float m_BoxHeight{};
};

extern InputUi g_InputUi;