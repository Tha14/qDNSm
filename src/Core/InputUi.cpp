#include "InputUi.hpp"
#include "Input.hpp"
#include "Renderer.hpp"
#include "Config.hpp"
#include <vsh/stdc.hpp>

InputUi g_InputUi;

static const char* TEXT_LAYOUT =
	"ABCDEFGHIJ"
	"KLMNOPQRST"
	"UVWXYZ0123"
	"456789 .-_";

static const int TEXT_COLUMNS = 10;
static const int TEXT_ROWS = 4;
static const char* NUMERIC_LAYOUT =
	"123"
	"456"
	"789"
	".0";

static const int NUMERIC_COLUMNS = 3;
static const int NUMERIC_ROWS = 4;

static const int MAX_TEXT_LENGTH = 63;

#ifdef ENABLE_GLYPH_BROWSER
/* Temporary glyph-browser constants - see BeginGlyphs in the header. */
static const unsigned int GLYPH_FIRST = 0xF000;
static const unsigned int GLYPH_LAST = 0xF8FF;
static const int GLYPH_COLUMNS = 16;
static const int GLYPH_ROWS = 8;
static const int GLYPH_PER_PAGE = GLYPH_COLUMNS * GLYPH_ROWS;
static const int GLYPH_PAGE_COUNT = (((GLYPH_LAST - GLYPH_FIRST + 1) / GLYPH_PER_PAGE) + 1);

static std::wstring HexLabel(const char* text)
{
	std::wstring out;
	for (const char* p = text; *p; p++)
		out += static_cast<wchar_t>(static_cast<unsigned char>(*p));

	return out;
}
#endif

static const float BOX_WIDTH = 900.0f;
static const float BOX_HEIGHT = 420.0f;
static const float CELL_HEIGHT = 56.0f;

/*
 * Vertical offsets for the text/numeric dialog. The renderer frame is y-up and centred on the
 * screen, so "from the top" is a subtraction from m_BoxMaxY.
 *
 * GRID_TOP_FROM_TOP matters most: the key grid is four rows (4 * CELL_HEIGHT = 224px) and has
 * to sit centred in the band between the value line and the footer. Anchoring it too high put
 * the first row's highlight box 16px inside the typed text.
 */
static const float TITLE_FROM_TOP = 30.0f;
static const float VALUE_FROM_TOP = 75.0f;
static const float FOOTER_FROM_BOTTOM = 30.0f;
static const float GRID_TOP_FROM_TOP = 180.0f;

static char ApplyCase(char character, bool upper)
{
	if (!upper && (character >= 'A') && (character <= 'Z'))
		return static_cast<char>(character - 'A' + 'a');

	return character;
}

static std::wstring Widen(const std::string& text)
{
	std::wstring out;
	for (size_t i = 0; i < text.size(); i++)
		out += static_cast<wchar_t>(static_cast<unsigned char>(text[i]));

	return out;
}

void InputUi::BeginText(const std::wstring& title, const std::string& initial, bool numeric)
{
	m_CreditLabel.clear();
	m_Credit.clear();


	m_Mode = numeric ? Mode_Numeric : Mode_Text;
	m_Title = title;
	m_Text = initial;
	m_Message.clear();

	m_Column = 0;
	m_Row = 0;
	m_Result = Result_None;
	m_HasResult = false;
}

void InputUi::BeginConfirm(const std::wstring& title, const std::string& message)
{
	m_CreditLabel.clear();
	m_Credit.clear();


	m_Mode = Mode_Confirm;
	m_Title = title;
	m_Message = Widen(message);
	m_Text.clear();

	m_ConfirmYes = false;
	m_Result = Result_None;
	m_HasResult = false;
}

#ifdef ENABLE_GLYPH_BROWSER
void InputUi::BeginGlyphs(unsigned int startCode)
{
	unsigned int first = (startCode < GLYPH_FIRST) ? GLYPH_FIRST : startCode;

	m_Mode = Mode_Glyphs;
	m_GlyphPage = static_cast<int>((first - GLYPH_FIRST) / GLYPH_PER_PAGE);
	m_GlyphIndex = static_cast<int>((first - GLYPH_FIRST) % GLYPH_PER_PAGE);
	m_Title = L"Glyph browser";
	m_Text.clear();
	m_Message.clear();
	m_Result = Result_None;
	m_HasResult = false;
}

#endif
void InputUi::ConsumeResult()
{
	m_HasResult = false;
	m_Result = Result_None;
}

void InputUi::AppendCharacter(char character)
{
	if (m_Text.size() >= static_cast<size_t>(MAX_TEXT_LENGTH))
		return;

	m_Text += character;
}

void InputUi::Backspace()
{
	if (m_Text.empty())
		return;

	m_Text.erase(m_Text.size() - 1);
}

bool InputUi::ConfirmButtonPressed()
{
	return g_Input.IsButtonDownFor(Input::BUTTON_CROSS, 250);
}

bool InputUi::CancelButtonPressed()
{
	return g_Input.IsButtonDownFor(Input::BUTTON_CIRCLE, 250);
}

bool InputUi::AcceptButtonPressed()
{
	return g_Input.IsButtonDownFor(Input::BUTTON_R1, 250) || g_Input.IsButtonDownFor(Input::BUTTON_START, 250);
}

void InputUi::Update()
{
	if (m_Mode == Mode_None)
		return;

	/*
	 * Read-only text closes on any button. Handled before the cancel check below so the reported
	 * result is consistent regardless of which button dismissed it.
	 *
	 * The result is Result_Info, not Result_Confirmed. See InputUi::Result_Info - reporting
	 * Confirmed here made this screen answer whichever flow happened to be in progress.
	 */
	if (m_Mode == Mode_Info)
	{
		if (CancelButtonPressed() || AcceptButtonPressed())
		{
			m_Result = Result_Info;
			m_Mode = Mode_None;
			m_HasResult = true;
		}

		return;
	}

	if (CancelButtonPressed())
	{
		m_Result = Result_Cancelled;
		m_Mode = Mode_None;
		m_HasResult = true;
		return;
	}

#ifdef ENABLE_GLYPH_BROWSER
	if (m_Mode == Mode_Glyphs)
	{
		if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_LEFT, 180))
			m_GlyphIndex = (m_GlyphIndex + GLYPH_PER_PAGE - 1) % GLYPH_PER_PAGE;

		if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_RIGHT, 180))
			m_GlyphIndex = (m_GlyphIndex + 1) % GLYPH_PER_PAGE;

		if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_UP, 180))
			m_GlyphIndex = (m_GlyphIndex + GLYPH_PER_PAGE - GLYPH_COLUMNS) % GLYPH_PER_PAGE;

		if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_DOWN, 180))
			m_GlyphIndex = (m_GlyphIndex + GLYPH_COLUMNS) % GLYPH_PER_PAGE;

		/* Page through: one page per press, four for coarse scrubbing. */
		int page = m_GlyphPage;

		if (g_Input.IsButtonDownFor(Input::BUTTON_L1, 250))
			page--;

		if (g_Input.IsButtonDownFor(Input::BUTTON_R1, 250))
			page++;

		if (g_Input.IsButtonDownFor(Input::BUTTON_L2, 250))
			page -= 4;

		if (g_Input.IsButtonDownFor(Input::BUTTON_R2, 250))
			page += 4;

		if (page < 0)
			page = 0;

		if (page >= GLYPH_PAGE_COUNT)
			page = GLYPH_PAGE_COUNT - 1;

		m_GlyphPage = page;

		if (ConfirmButtonPressed())
		{
			m_Result = Result_Confirmed;
			m_Mode = Mode_None;
			m_HasResult = true;
		}

		return;
	}

#endif

	if (m_Mode == Mode_Confirm)
	{
		if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_LEFT, 250) || g_Input.IsButtonDownFor(Input::BUTTON_PAD_RIGHT, 250))
			m_ConfirmYes = !m_ConfirmYes;

		if (ConfirmButtonPressed())
		{
			m_Result = m_ConfirmYes ? Result_Confirmed : Result_Cancelled;
			m_Mode = Mode_None;
			m_HasResult = true;
		}

		return;
	}

	int columns = (m_Mode == Mode_Numeric) ? NUMERIC_COLUMNS : TEXT_COLUMNS;
	int rows = (m_Mode == Mode_Numeric) ? NUMERIC_ROWS : TEXT_ROWS;

	if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_LEFT, 180))
		m_Column = (m_Column + columns - 1) % columns;

	if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_RIGHT, 180))
		m_Column = (m_Column + 1) % columns;

	/* The column is deliberately preserved when moving between rows - every row in both
	 * grids has the same width, so there is no column to clamp. Resetting it made typing
	 * an address mean re-counting across from the left on every line. */
	if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_UP, 180))
		m_Row = (m_Row + rows - 1) % rows;

	if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_DOWN, 180))
		m_Row = (m_Row + 1) % rows;

	if (g_Input.IsButtonDownFor(Input::BUTTON_TRIANGLE, 250))
		Backspace();

	if (m_Mode == Mode_Text)
	{
		if (g_Input.IsButtonDownFor(Input::BUTTON_L1, 250))
			m_UpperCase = !m_UpperCase;
	}

	if (ConfirmButtonPressed())
	{
		const char* layout = (m_Mode == Mode_Numeric) ? NUMERIC_LAYOUT : TEXT_LAYOUT;
		int index = (m_Row * columns) + m_Column;

		/*
		 * The numeric grid's last row is ".0" - two characters, not three - so column 2 of
		 * that row lands on the terminator. Without this guard X there appended a NUL byte
		 * into the IP string, which then failed IsValidIpv4.
		 */
		if (index < static_cast<int>(vsh::strlen(layout)))
		{
			char character = layout[index];

			// The numeric pad has no letters, so case folding is text-only.
			if (m_Mode == Mode_Text)
				character = ApplyCase(character, m_UpperCase);

			AppendCharacter(character);
		}
	}

	if (AcceptButtonPressed())
	{
		if (!m_Text.empty())
		{
			m_Result = Result_Confirmed;
			m_Mode = Mode_None;
			m_HasResult = true;
		}
	}
}

void InputUi::DrawBox(float width, float height)
{
	float viewportWidth = g_Renderer.m_ViewportWidth;
	float viewportHeight = g_Renderer.m_ViewportHeight;

	m_BoxWidth = width;
	m_BoxHeight = height;

	// Origin is the screen centre and y grows upward, so the visual top is +height/2.
	m_BoxLeft = -width / 2.0f;
	m_BoxMinY = -height / 2.0f;
	m_BoxMaxY = height / 2.0f;

	g_Renderer.Rectangle(
		paf::vec2(0.0f, 0.0f),
		paf::vec2(viewportWidth, viewportHeight),
		paf::vec4(0.0f, 0.0f, 0.0f, 0.75f),
		Renderer::Centered, Renderer::Centered);

	g_Renderer.Rectangle(
		paf::vec2(0.0f, 0.0f),
		paf::vec2(m_BoxWidth, m_BoxHeight),
		paf::vec4(0.05f, 0.05f, 0.08f, 0.95f),
		Renderer::Centered, Renderer::Centered);
}

void InputUi::DrawTitle()
{
	g_Renderer.Text(m_Title, paf::vec2(m_BoxLeft + 30.0f, m_BoxMaxY - TITLE_FROM_TOP), 24.0f,
		paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Left, Renderer::Centered);

	if (m_Mode == Mode_Text)
	{
		const wchar_t* mode = m_UpperCase ? L"ABC" : L"abc";

		g_Renderer.Text(mode, paf::vec2(m_BoxLeft + m_BoxWidth - 30.0f, m_BoxMaxY - TITLE_FROM_TOP), 24.0f,
			paf::vec4(0.0f, 1.0f, 1.0f, 1.0f), Renderer::Right, Renderer::Centered);
	}
}

void InputUi::DrawValue()
{
	g_Renderer.Text(Widen(m_Text), paf::vec2(m_BoxLeft + 30.0f, m_BoxMaxY - VALUE_FROM_TOP), 30.0f,
		paf::vec4(0.0f, 1.0f, 1.0f, 1.0f), Renderer::Left, Renderer::Centered);
}

void InputUi::DrawSelection(float centerX, float centerY, float w, float h)
{
	// Matches Menu::DrawHighlightBar - a rectangle is positioned by its CENTRE.
	g_Renderer.Rectangle(paf::vec2(centerX, centerY), paf::vec2(w, h),
		paf::vec4(0.0f, 1.0f, 1.0f, 0.35f), Renderer::Centered, Renderer::Centered);
}

void InputUi::DrawFooter(const wchar_t* left, const wchar_t* right)
{
	g_Renderer.Text(left, paf::vec2(m_BoxLeft + 30.0f, m_BoxMinY + FOOTER_FROM_BOTTOM), 16.0f,
		paf::vec4(0.7f, 0.7f, 0.7f, 1.0f), Renderer::Left, Renderer::Centered);

	g_Renderer.Text(right, paf::vec2(m_BoxLeft + m_BoxWidth - 30.0f, m_BoxMinY + FOOTER_FROM_BOTTOM), 16.0f,
		paf::vec4(0.7f, 0.7f, 0.7f, 1.0f), Renderer::Right, Renderer::Centered);
}

void InputUi::DrawCharacterGrid()
{
	float cellWidth = 78.0f;
	float cellHeight = CELL_HEIGHT;
	float gridWidth = cellWidth * TEXT_COLUMNS;

	float startX = m_BoxLeft + ((m_BoxWidth - gridWidth) / 2.0f);
	float startY = m_BoxMaxY - GRID_TOP_FROM_TOP;

	for (int row = 0; row < TEXT_ROWS; row++)
	{
		for (int column = 0; column < TEXT_COLUMNS; column++)
		{
			int index = (row * TEXT_COLUMNS) + column;

			/* Show the letter case the next typed character will actually use, rather than
			 * making the reader infer it from the ABC/abc badge. */
			char character = ApplyCase(TEXT_LAYOUT[index], m_UpperCase);

			if (character == '\0')
				continue;

			float x = startX + (column * cellWidth);
			float y = startY - (row * cellHeight);

			if (row == m_Row && column == m_Column)
				DrawSelection(x + (cellWidth / 2.0f), y + (cellHeight / 2.0f), cellWidth, cellHeight);

			std::wstring glyph;
			glyph += static_cast<wchar_t>(static_cast<unsigned char>(character));

			g_Renderer.Text(glyph, paf::vec2(x + (cellWidth / 2.0f), y + (cellHeight / 2.0f)), 26.0f,
				paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);
		}
	}
}

void InputUi::DrawNumericPad()
{
	float cellWidth = 110.0f;
	float cellHeight = CELL_HEIGHT;
	float gridWidth = cellWidth * NUMERIC_COLUMNS;

	float startX = m_BoxLeft + ((m_BoxWidth - gridWidth) / 2.0f);
	float startY = m_BoxMaxY - GRID_TOP_FROM_TOP;

	for (int row = 0; row < NUMERIC_ROWS; row++)
	{
		for (int column = 0; column < NUMERIC_COLUMNS; column++)
		{
			char character = NUMERIC_LAYOUT[(row * NUMERIC_COLUMNS) + column];
			if (character == '\0')
				continue;

			float x = startX + (column * cellWidth);
			float y = startY - (row * cellHeight);

			if (row == m_Row && column == m_Column)
				DrawSelection(x + (cellWidth / 2.0f), y + (cellHeight / 2.0f), cellWidth, cellHeight);

			std::wstring glyph;
			glyph += static_cast<wchar_t>(static_cast<unsigned char>(character));

			g_Renderer.Text(glyph, paf::vec2(x + (cellWidth / 2.0f), y + (cellHeight / 2.0f)), 28.0f,
				paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);
		}
	}
}

void InputUi::DrawConfirm()
{
	g_Renderer.Text(m_Message, paf::vec2(m_BoxLeft + (m_BoxWidth / 2.0f), m_BoxMaxY - 110.0f), 24.0f,
		paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);

	float buttonWidth = 180.0f;
	float buttonHeight = 60.0f;
	float gap = 40.0f;
	float totalWidth = (buttonWidth * 2.0f) + gap;
	float startX = m_BoxLeft + (m_BoxWidth - totalWidth) / 2.0f;
	float y = m_BoxMaxY - 190.0f;

	float noX = startX;
	float yesX = startX + buttonWidth + gap;

	if (m_ConfirmYes)
		DrawSelection(yesX + (buttonWidth / 2.0f), y + (buttonHeight / 2.0f), buttonWidth, buttonHeight);
	else
		DrawSelection(noX + (buttonWidth / 2.0f), y + (buttonHeight / 2.0f), buttonWidth, buttonHeight);

	g_Renderer.Text(L"No", paf::vec2(noX + (buttonWidth / 2.0f), y + (buttonHeight / 2.0f)), 24.0f,
		paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);

	g_Renderer.Text(L"Yes", paf::vec2(yesX + (buttonWidth / 2.0f), y + (buttonHeight / 2.0f)), 24.0f,
		paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);
}

void InputUi::BeginInfo(const std::wstring& title, const std::string& message,
	const std::wstring& creditLabel, const std::wstring& credit)
{
	m_Mode = Mode_Info;
	m_Title = title;
	m_Message = Widen(message);
	m_CreditLabel = creditLabel;
	m_Credit = credit;
	m_Text.clear();
	m_HasResult = false;
}

/*
 * Multi-line body text.
 *
 * Split by hand rather than with std::getline because m_Message is a std::wstring on this
 * toolchain and the stream header pulls in machinery this module does not otherwise need. One pass
 * over the buffer, no allocation per line.
 *
 * An empty line is drawn as a paragraph break rather than skipped, so the caller's text controls
 * its own spacing. Lines are clipped to the box rather than wrapped: wrapping needs font metrics
 * this module has no way to query, and a hand-measured wrap would be wrong at any other text size.
 * Callers therefore keep their lines short.
 */
void InputUi::DrawInfo()
{
	const float lineHeight = 28.0f;
	const float paragraphGap = 12.0f;

	float y = m_BoxMaxY - 90.0f;

	std::size_t lineStart = 0;

	for (std::size_t i = 0; i <= m_Message.size(); i++)
	{
		const bool atEnd = (i == m_Message.size());

		if (!atEnd && m_Message[i] != L'\n')
			continue;

		const std::wstring line = m_Message.substr(lineStart, i - lineStart);

		if (!line.empty())
		{
			/* Stop rather than draw outside the box. */
			if (y < m_BoxMinY + 60.0f)
				break;

			g_Renderer.Text(line, paf::vec2(m_BoxLeft + 40.0f, y), 22.0f,
				paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Left, Renderer::Centered);
		}

		y -= lineHeight + (line.empty() ? paragraphGap : 0.0f);
		lineStart = i + 1;
	}

	/*
	 * The credit block: a small dim label above a larger accent-coloured name, both centred on the
	 * same axis so the label reads as belonging to the name rather than to the paragraph above.
	 *
	 * Roughly twice the body size for the name, with the drop shadow left on. That is as far as this
	 * can go: the PS3 has several system faces and paf selects between them through
	 * PhFont::GlyphType, but the vendored paf.hpp declares PhFont nowhere - it appears only in
	 * demangled comments - so the enum's values cannot be read from this SDK and a guessed one would
	 * render the wrong face or nothing. Renderer::Text carries a GlyphSet parameter for when that is
	 * recovered; nothing sets it to anything but the default yet, deliberately.
	 */
	if (!m_Credit.empty() || !m_CreditLabel.empty())
	{
		if (!m_CreditLabel.empty())
		{
			g_Renderer.Text(m_CreditLabel, paf::vec2(0.0f, m_BoxMinY + 118.0f), 20.0f,
				paf::vec4(0.65f, 0.65f, 0.70f, 1.0f), Renderer::Centered, Renderer::Centered);
		}

		if (!m_Credit.empty())
		{
			g_Renderer.Text(m_Credit, paf::vec2(0.0f, m_BoxMinY + 62.0f), 40.0f,
				paf::vec4(0.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);
		}
	}
}

#ifdef ENABLE_GLYPH_BROWSER
void InputUi::DrawGlyphBrowser()
{
	float cellWidth = 44.0f;
	float cellHeight = 44.0f;
	float gridWidth = cellWidth * GLYPH_COLUMNS;
	float gridHeight = cellHeight * GLYPH_ROWS;

	float startX = m_BoxLeft + ((m_BoxWidth - gridWidth) / 2.0f);
	float startY = m_BoxMaxY - 110.0f;

	unsigned int base = GLYPH_FIRST + (unsigned int)(m_GlyphPage * GLYPH_PER_PAGE);

	for (int row = 0; row < GLYPH_ROWS; row++)
	{
		for (int column = 0; column < GLYPH_COLUMNS; column++)
		{
			int index = (row * GLYPH_COLUMNS) + column;

			float x = startX + (column * cellWidth);
			float y = startY - (row * cellHeight);

			if (index == m_GlyphIndex)
				DrawSelection(x + (cellWidth / 2.0f), y + (cellHeight / 2.0f), cellWidth, cellHeight);

			std::wstring glyph;
			glyph += static_cast<wchar_t>(static_cast<unsigned short>(base + index));

			g_Renderer.Text(glyph, paf::vec2(x + (cellWidth / 2.0f), y + (cellHeight / 2.0f)), 26.0f,
				paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);
		}
	}

	unsigned int selected = base + (unsigned int)m_GlyphIndex;
	char code[8];

	const char* digits = "0123456789ABCDEF";
	for (int i = 0; i < 4; i++)
		code[i] = digits[(selected >> (12 - (i * 4))) & 0xF];

	code[4] = 0;

	std::wstring label = L"CODE ";
	for (int i = 0; i < 4; i++)
		label += static_cast<wchar_t>(static_cast<unsigned char>(code[i]));

	g_Renderer.Text(label, paf::vec2(m_BoxLeft + (m_BoxWidth / 2.0f), startY - gridHeight - 60.0f), 34.0f,
		paf::vec4(0.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);

	char pageLabel[32];
	vsh::snprintf(pageLabel, sizeof(pageLabel), "PAGE %d / %d", m_GlyphPage + 1, GLYPH_PAGE_COUNT);

	g_Renderer.Text(HexLabel(pageLabel), paf::vec2(m_BoxLeft + 30.0f, m_BoxMinY + 30.0f), 16.0f,
		paf::vec4(0.7f, 0.7f, 0.7f, 1.0f), Renderer::Left, Renderer::Centered);
}

#endif
void InputUi::Draw()
{
	if (m_Mode == Mode_None)
		return;

	if (m_Mode == Mode_Confirm)
		DrawBox(700.0f, 320.0f);
	else if (m_Mode == Mode_Info)
		DrawBox(760.0f, 460.0f);
	else
		DrawBox(BOX_WIDTH, BOX_HEIGHT);

#ifdef ENABLE_GLYPH_BROWSER
	if (m_Mode == Mode_Glyphs)
	{
		DrawTitle();
		DrawGlyphBrowser();
		DrawFooter(L"L1/R1: page    L2/R2: skip 4", L"Circle: back    X: choose");
		return;
	}
#endif

	DrawTitle();

	if (m_Mode == Mode_Confirm)
	{
		DrawConfirm();
		DrawFooter(L"X: select    Circle: cancel", L"");
		return;
	}

	/* Read-only text. Nothing here can be accepted or rejected, so any of the three buttons
	 * closes it rather than pretending there was a choice to make. */
	if (m_Mode == Mode_Info)
	{
		DrawInfo();
		DrawFooter(L"X or Circle: close", L"");
		return;
	}

	DrawValue();

	if (m_Mode == Mode_Numeric)
		DrawNumericPad();
	else
		DrawCharacterGrid();

	DrawFooter(L"X: type    Triangle: backspace    L1: case", L"Circle: cancel    R1/Start: done");
}