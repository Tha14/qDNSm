#include "InputUi.hpp"
#include "Input.hpp"
#include "Renderer.hpp"
#include "Config.hpp"
#include "Language.hpp"
#include "Utils/Std.hpp"
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

static const float BOX_WIDTH = 900.0f;
static const float BOX_HEIGHT = 420.0f;
static const float CELL_HEIGHT = 56.0f;

static const float DIRECTORY_WIDTH = 1180.0f;
static const float DIRECTORY_HEIGHT = 620.0f;
static const float DIRECTORY_SEARCH_FROM_TOP = 58.0f;
static const float DIRECTORY_SEARCH_HEIGHT = 42.0f;
static const float DIRECTORY_STATUS_FROM_TOP = 112.0f;
static const float DIRECTORY_LIST_FROM_TOP = 132.0f;
static const float DIRECTORY_ROW_HEIGHT = 42.0f;
static const float DIRECTORY_GRID_TOP_FROM_TOP = 160.0f;
static const int DIRECTORY_VISIBLE_ROWS = 9;
static const int DIRECTORY_MIN_SEARCH_LENGTH = 3;
static const size_t DIRECTORY_DETAIL_TEXT_LIMIT = 110;
static const size_t DIRECTORY_IP_RESERVE = 24;
static const float DIRECTORY_TAG_HEIGHT = 18.0f;
static const float DIRECTORY_TAG_PADDING = 6.0f;
static const float DIRECTORY_TAG_GAP = 6.0f;
static const float DIRECTORY_TAG_TEXT_HEIGHT = 11.0f;
static const size_t DIRECTORY_NOTE_WIDTH = 88;
static const size_t DIRECTORY_NOTE_MAX_LINES = 5;
static const float DIALOG_BORDER_WIDTH = 2.0f;
static const wchar_t kDirectoryTagPatch[] = L"Requires patch";
static const wchar_t kDirectoryTagWhitelist[] = L"Requires whitelisting";
static const wchar_t kDirectoryTagDiscord[] = L"Discord Account Linking";

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

static paf::vec4 MutedBlue(float alpha)
{
	return paf::vec4(0.05f, 0.48f, 0.70f, alpha);
}

static std::wstring Widen(const std::string& text)
{
	std::wstring out;
	for (size_t i = 0; i < text.size(); i++)
		out += static_cast<wchar_t>(static_cast<unsigned char>(text[i]));

	return out;
}

static std::wstring Truncate(const std::wstring& text, size_t maximum)
{
	if (text.size() <= maximum)
		return text;

	if (maximum <= 3)
		return text.substr(0, maximum);

	return text.substr(0, maximum - 3) + L"...";
}

static std::wstring Join(const std::vector<std::wstring>& values, const wchar_t* separator)
{
	std::wstring out;

	for (size_t i = 0; i < values.size(); i++)
	{
		if (i != 0)
			out += separator;

		out += values[i];
	}

	return out;
}

static size_t DirectoryHiddenLength(size_t hidden)
{
	size_t digits = 1;

	while (hidden >= 10)
	{
		hidden /= 10;
		digits++;
	}

	return 2 + digits;
}

static std::wstring FormatDirectoryProjects(const std::vector<std::wstring>& projects,
	size_t maximum)
{
	if (projects.empty())
		return std::wstring();

	const size_t reserve = (projects.size() > 1)
		? DirectoryHiddenLength(projects.size() - 1) : 0;
	const size_t budget = (maximum > reserve) ? maximum - reserve : 0;
	std::wstring out = projects[0];

	if (out.size() > budget)
		out = Truncate(out, budget);

	for (size_t i = 1; i < projects.size(); i++)
	{
		std::wstring candidate = out + L", " + projects[i];

		if (candidate.size() > budget)
		{
			out += L" +";
			out += to_wstring(static_cast<int>(projects.size() - i));
			return out;
		}

		out.swap(candidate);
	}

	return out;
}

static std::wstring FormatDirectoryIps(const std::vector<std::string>& ips, size_t maximum)
{
	if (ips.empty())
		return std::wstring();

	const size_t reserve = (ips.size() > 1)
		? DirectoryHiddenLength(ips.size() - 1) : 0;
	const size_t budget = (maximum > reserve) ? maximum - reserve : 0;
	std::wstring out = Widen(ips[0]);

	if (out.size() > budget)
		out = Truncate(out, budget);

	for (size_t i = 1; i < ips.size(); i++)
	{
		std::wstring candidate = out + L", " + Widen(ips[i]);

		if (candidate.size() > budget)
		{
			out += L" +";
			out += to_wstring(static_cast<int>(ips.size() - i));
			return out;
		}

		out.swap(candidate);
	}

	return out;
}

static std::wstring FormatDirectoryDetail(const GameDirectoryRow& row)
{
	std::wstring allIps;

	for (size_t i = 0; i < row.ips.size(); i++)
	{
		if (i != 0)
			allIps += L", ";

		allIps += Widen(row.ips[i]);
	}

	std::wstring full = Join(row.projects, L", ");

	if (!allIps.empty())
	{
		if (!full.empty())
			full += L" - ";

		full += allIps;
	}

	if (full.size() <= DIRECTORY_DETAIL_TEXT_LIMIT)
		return full;

	const size_t projectLimit = row.ips.empty()
		? DIRECTORY_DETAIL_TEXT_LIMIT
		: (DIRECTORY_DETAIL_TEXT_LIMIT - DIRECTORY_IP_RESERVE);
	std::wstring detail = FormatDirectoryProjects(row.projects, projectLimit);

	if (!row.ips.empty())
	{
		if (!detail.empty())
			detail += L" - ";

		const size_t used = detail.size();
		const size_t ipLimit = (used < DIRECTORY_DETAIL_TEXT_LIMIT)
			? (DIRECTORY_DETAIL_TEXT_LIMIT - used) : 0;

		detail += FormatDirectoryIps(row.ips, ipLimit);
	}

	return detail;
}

static void WrapDirectoryLine(const std::wstring& text, size_t maximum,
	std::vector<std::wstring>& lines)
{
	std::wstring current;
	size_t cursor = 0;

	while (cursor < text.size())
	{
		while ((cursor < text.size()) && (text[cursor] == L' '))
			cursor++;

		size_t start = cursor;

		while ((cursor < text.size()) && (text[cursor] != L' '))
			cursor++;

		if (start == cursor)
			break;

		std::wstring word = text.substr(start, cursor - start);

		if (current.empty())
			current = word;
		else if ((current.size() + 1 + word.size()) <= maximum)
		{
			current += L' ';
			current += word;
		}
		else
		{
			lines.push_back(current);
			current = word;
		}
	}

	if (!current.empty())
		lines.push_back(current);
	else if (text.empty())
		lines.push_back(text);
}

static std::vector<std::wstring> WrapDirectoryText(const std::wstring& text, size_t maximum)
{
	std::vector<std::wstring> lines;
	size_t start = 0;

	while (true)
	{
		size_t end = text.find(L'\n', start);
		std::wstring line = (end == std::wstring::npos)
			? text.substr(start)
			: text.substr(start, end - start);

		WrapDirectoryLine(line, maximum, lines);

		if (end == std::wstring::npos)
			break;

		start = end + 1;
	}

	return lines;
}

static float DirectoryTagWidth(const std::wstring& label)
{
	float width = g_Renderer.MeasureText(label, DIRECTORY_TAG_TEXT_HEIGHT);
	const float estimated = static_cast<float>(label.size()) *
		DIRECTORY_TAG_TEXT_HEIGHT * 0.52f;

	if (width < estimated)
		width = estimated;

	return width + (DIRECTORY_TAG_PADDING * 2.0f);
}

static void DrawDirectoryTag(const std::wstring& label, float width, float x, float centerY,
	const paf::vec4& fill)
{
	const float centerX = x + (width / 2.0f);

	g_Renderer.Rectangle(paf::vec2(centerX, centerY),
		paf::vec2(width, DIRECTORY_TAG_HEIGHT), paf::vec4(0.0f, 0.0f, 0.0f, 0.55f),
		Renderer::Centered, Renderer::Centered);
	g_Renderer.Rectangle(paf::vec2(centerX, centerY),
		paf::vec2(width - 2.0f, DIRECTORY_TAG_HEIGHT - 2.0f), fill,
		Renderer::Centered, Renderer::Centered);

	const paf::vec4 textColor(1.0f, 1.0f, 1.0f, 1.0f);
	const paf::vec2 textPosition(centerX, centerY);

	g_Renderer.Text(label, textPosition, DIRECTORY_TAG_TEXT_HEIGHT, textColor,
		Renderer::Centered, Renderer::Centered);
	g_Renderer.Text(label, paf::vec2(centerX + 0.4f, centerY), DIRECTORY_TAG_TEXT_HEIGHT,
		textColor, Renderer::Centered, Renderer::Centered, 0.0f, false);
}

static float DirectoryTagsWidth(uint32_t tags, float patchWidth, float whitelistWidth,
	float discordWidth)
{
	float width = 0.0f;

	if ((tags & GameTag_RequiresPatch) != 0)
		width = patchWidth;

	if ((tags & GameTag_RequiresWhitelist) != 0)
	{
		if (width > 0.0f)
			width += DIRECTORY_TAG_GAP;

		width += whitelistWidth;
	}

	if ((tags & GameTag_DiscordAccountLinking) != 0)
	{
		if (width > 0.0f)
			width += DIRECTORY_TAG_GAP;

		width += discordWidth;
	}

	return width;
}

static void DrawDirectoryTags(uint32_t tags, float x, float centerY,
	const std::wstring& patchLabel, float patchWidth,
	const std::wstring& whitelistLabel, float whitelistWidth,
	const std::wstring& discordLabel, float discordWidth)
{
	const paf::vec4 patchColor = MutedBlue(0.95f);
	const paf::vec4 whitelistColor(0.82f, 0.45f, 0.08f, 0.95f);
	const paf::vec4 discordColor(0.42f, 0.28f, 0.68f, 0.95f);

	if ((tags & GameTag_RequiresPatch) != 0)
	{
		DrawDirectoryTag(patchLabel, patchWidth, x, centerY, patchColor);
		x += patchWidth + DIRECTORY_TAG_GAP;
	}

	if ((tags & GameTag_RequiresWhitelist) != 0)
	{
		DrawDirectoryTag(whitelistLabel, whitelistWidth, x, centerY, whitelistColor);
		x += whitelistWidth + DIRECTORY_TAG_GAP;
	}

	if ((tags & GameTag_DiscordAccountLinking) != 0)
		DrawDirectoryTag(discordLabel, discordWidth, x, centerY, discordColor);
}

static std::wstring DirectoryNoteLabel(const GameDirectoryNote& note)
{
	std::wstring label = note.server
		? T(TEXT_DIRECTORY_SERVER_NOTE) : T(TEXT_DIRECTORY_NOTE);

	if (!note.project.empty())
	{
		label += L" [";
		label += note.project;
		label += L"]";
	}

	label += L": ";
	return label;
}

static void DrawDirectoryNotes(const GameDirectoryRow& row, float x, float& y, float minimumY)
{
	size_t linesLeft = DIRECTORY_NOTE_MAX_LINES;

	for (size_t i = 0; (i < row.notes.size()) && (linesLeft > 0); i++)
	{
		std::wstring text = DirectoryNoteLabel(row.notes[i]);
		text += row.notes[i].text;

		std::vector<std::wstring> lines = WrapDirectoryText(text, DIRECTORY_NOTE_WIDTH);

		for (size_t j = 0; (j < lines.size()) && (linesLeft > 0); j++)
		{
			if (y < minimumY)
				return;

			g_Renderer.Text(Truncate(lines[j], DIRECTORY_NOTE_WIDTH + 2),
				paf::vec2(x, y), 13.0f,
				paf::vec4(0.72f, 0.72f, 0.76f, 1.0f), Renderer::Left, Renderer::Centered);

			y -= 18.0f;
			linesLeft--;
		}
	}
}

void InputUi::BeginText(const std::wstring& title, const std::string& initial, bool numeric)
{
	m_InfoHeading.clear();
	m_InfoCenterBody = false;
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

void InputUi::BeginConfirm(const std::wstring& title, const std::wstring& message)
{
	m_InfoHeading.clear();
	m_InfoCenterBody = false;
	m_CreditLabel.clear();
	m_Credit.clear();


	m_Mode = Mode_Confirm;
	m_Title = title;
	m_Message = message;
	m_Text.clear();

	m_ConfirmYes = false;
	m_Result = Result_None;
	m_HasResult = false;
}

void InputUi::ConsumeResult()
{
	m_HasResult = false;
	m_Result = Result_None;
}

void InputUi::Cancel()
{
	if (m_Mode == Mode_None)
		return;

	if (m_Mode == Mode_Directory)
		ClearDirectory();

	m_Mode = Mode_None;
	m_Result = Result_Cancelled;
	m_HasResult = true;
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

	if (m_Mode == Mode_Directory)
	{
		UpdateDirectory();
		return;
	}

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

	const bool textMode = (m_Mode == Mode_Text);
	int columns = textMode ? TEXT_COLUMNS : NUMERIC_COLUMNS;
	int rows = textMode ? TEXT_ROWS : NUMERIC_ROWS;
	const char* layout = textMode ? TEXT_LAYOUT : NUMERIC_LAYOUT;

	UpdateTextGridMovement(columns, rows, textMode);
	TypeTextGridCharacter(layout, columns, textMode);

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

void InputUi::UpdateTextGridMovement(int columns, int rows, bool allowCase)
{
	if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_LEFT, 180))
		m_Column = (m_Column + columns - 1) % columns;

	if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_RIGHT, 180))
		m_Column = (m_Column + 1) % columns;

	if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_UP, 180))
		m_Row = (m_Row + rows - 1) % rows;

	if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_DOWN, 180))
		m_Row = (m_Row + 1) % rows;

	if (g_Input.IsButtonDownFor(Input::BUTTON_SQUARE, 250))
		Backspace();

	if (allowCase && g_Input.IsButtonDownFor(Input::BUTTON_L1, 250))
		m_UpperCase = !m_UpperCase;
}

void InputUi::TypeTextGridCharacter(const char* layout, int columns, bool applyCase)
{
	if (!ConfirmButtonPressed())
		return;

	int index = (m_Row * columns) + m_Column;

	if (index >= static_cast<int>(vsh::strlen(layout)))
		return;

	char character = layout[index];

	if (applyCase)
		character = ApplyCase(character, m_UpperCase);

	AppendCharacter(character);
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
		paf::vec2(m_BoxWidth + (DIALOG_BORDER_WIDTH * 2.0f),
			m_BoxHeight + (DIALOG_BORDER_WIDTH * 2.0f)),
		MutedBlue(0.95f),
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

void InputUi::DrawCharacterGrid(float topFromTop)
{
	float cellWidth = 78.0f;
	float cellHeight = CELL_HEIGHT;
	float gridWidth = cellWidth * TEXT_COLUMNS;

	float startX = m_BoxLeft + ((m_BoxWidth - gridWidth) / 2.0f);
	float startY = m_BoxMaxY - topFromTop;

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

	g_Renderer.Text(T(TEXT_INPUT_NO), paf::vec2(noX + (buttonWidth / 2.0f), y + (buttonHeight / 2.0f)), 24.0f,
		paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);

	g_Renderer.Text(T(TEXT_INPUT_YES), paf::vec2(yesX + (buttonWidth / 2.0f), y + (buttonHeight / 2.0f)), 24.0f,
		paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);
}

void InputUi::BeginInfo(const std::wstring& title, const std::wstring& message,
	const std::wstring& creditLabel, const std::wstring& credit,
	const std::wstring& heading, bool centerBody)
{
	m_Mode = Mode_Info;
	m_Title = title;
	m_Message = message;
	m_InfoHeading = heading;
	m_InfoCenterBody = centerBody;
	m_CreditLabel = creditLabel;
	m_Credit = credit;
	m_Text.clear();
	m_HasResult = false;
}

void InputUi::BeginDirectory(GameListLoadResult result)
{
	m_InfoHeading.clear();
	m_InfoCenterBody = false;
	m_CreditLabel.clear();
	m_Credit.clear();

	m_Mode = Mode_Directory;
	m_Title = T(TEXT_MENU_GAME_DIRECTORY);
	m_Message.clear();
	m_Text.clear();
	m_Result = Result_None;
	m_HasResult = false;

	m_DirectoryLoadResult = result;
	m_DirectoryQuery.clear();
	m_DirectoryGrouped = false;
	m_DirectorySearchFocused = true;
	m_DirectoryEditing = false;
	m_DirectoryDetail = false;
	m_DirectoryDetailRow = 0;
	m_DirectorySelected = 0;
	m_DirectoryScroll = 0;

	m_Column = 0;
	m_Row = 0;
	m_UpperCase = true;

	/*
	 * The row list stays empty until the query reaches the minimum length, so the directory
	 * opens on a blank screen rather than dumping the whole catalogue into memory at once.
	 */
	m_DirectoryRows.clear();
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
	const float lineHeight = 22.0f;
	const float paragraphGap = 8.0f;

	float y = m_BoxMaxY - 90.0f;

	if (!m_InfoHeading.empty())
	{
		g_Renderer.Text(m_InfoHeading, paf::vec2(0.0f, m_BoxMaxY - 90.0f), 40.0f,
			paf::vec4(0.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);

		y = m_BoxMaxY - 130.0f;
	}

	const float bodyX = m_InfoCenterBody
		? (m_BoxLeft + (m_BoxWidth / 2.0f))
		: (m_BoxLeft + 40.0f);
	const Renderer::Align bodyAlign = m_InfoCenterBody ? Renderer::Centered : Renderer::Left;

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

			g_Renderer.Text(line, paf::vec2(bodyX, y), 18.0f,
				paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), bodyAlign, Renderer::Centered);
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
			g_Renderer.Text(m_CreditLabel, paf::vec2(0.0f, m_BoxMinY + 102.0f), 20.0f,
				paf::vec4(0.65f, 0.65f, 0.70f, 1.0f), Renderer::Centered, Renderer::Centered);
		}

		if (!m_Credit.empty())
		{
			g_Renderer.Text(m_Credit, paf::vec2(0.0f, m_BoxMinY + 62.0f), 40.0f,
				paf::vec4(0.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);
		}
	}
}

void InputUi::BeginDirectoryEdit()
{
	m_Text = m_DirectoryQuery;
	m_Column = 0;
	m_Row = 0;
	m_UpperCase = true;
	m_DirectoryEditing = true;
	m_DirectorySearchFocused = true;
}

void InputUi::CancelDirectoryEdit()
{
	m_Text.clear();
	m_DirectoryEditing = false;
	m_DirectorySearchFocused = true;
}

void InputUi::ClearDirectory()
{
	GameDirectoryRows emptyRows;
	m_DirectoryRows.swap(emptyRows);
}

void InputUi::ApplyDirectoryEdit()
{
	m_DirectoryQuery = m_Text;
	m_Text.clear();
	m_DirectoryEditing = false;
	m_DirectorySelected = 0;
	m_DirectoryScroll = 0;
	RebuildDirectoryRows();
	m_DirectorySearchFocused = m_DirectoryRows.empty();
}

void InputUi::RebuildDirectoryRows()
{
	m_DirectoryRows.clear();

	if (m_DirectoryLoadResult != GameList_Loaded)
		return;

	if (m_DirectoryQuery.size() < static_cast<size_t>(DIRECTORY_MIN_SEARCH_LENGTH))
		return;

	bool truncated = false;
	GameListLoadResult result = GameDirectory_Search(g_Config.m_DnsServers,
		m_DirectoryQuery, m_DirectoryGrouped, m_DirectoryRows, truncated);

	if (result != GameList_Loaded)
	{
		m_DirectoryLoadResult = result;
		m_DirectoryRows.clear();
		return;
	}

	if (m_DirectoryRows.empty())
	{
		m_DirectorySelected = 0;
		m_DirectoryScroll = 0;
		return;
	}

	if (m_DirectorySelected >= m_DirectoryRows.size())
		m_DirectorySelected = m_DirectoryRows.size() - 1;

	const size_t visible = static_cast<size_t>(DIRECTORY_VISIBLE_ROWS);

	if (m_DirectoryScroll > m_DirectoryRows.size() - 1)
		m_DirectoryScroll = m_DirectoryRows.size() - 1;

	if (m_DirectorySelected < m_DirectoryScroll)
		m_DirectoryScroll = m_DirectorySelected;
	else if (m_DirectorySelected >= m_DirectoryScroll + visible)
		m_DirectoryScroll = m_DirectorySelected - visible + 1;
}

void InputUi::EnsureDirectoryVisible()
{
	if (m_DirectoryRows.empty())
		return;

	const size_t visible = static_cast<size_t>(DIRECTORY_VISIBLE_ROWS);

	if (m_DirectorySelected < m_DirectoryScroll)
		m_DirectoryScroll = m_DirectorySelected;
	else if (m_DirectorySelected >= m_DirectoryScroll + visible)
		m_DirectoryScroll = m_DirectorySelected - visible + 1;
}

void InputUi::MoveDirectorySelection(int direction)
{
	if (m_DirectoryRows.empty())
		return;

	if (direction < 0)
	{
		if (m_DirectorySelected > 0)
			m_DirectorySelected--;
	}
	else
	{
		if ((m_DirectorySelected + 1) < m_DirectoryRows.size())
			m_DirectorySelected++;
	}

	EnsureDirectoryVisible();
}

void InputUi::PageDirectory(int direction)
{
	if (m_DirectoryRows.empty())
		return;

	const size_t visible = static_cast<size_t>(DIRECTORY_VISIBLE_ROWS);
	const size_t maximum = (m_DirectoryRows.size() > visible)
		? (m_DirectoryRows.size() - visible) : 0;

	if (direction < 0)
	{
		m_DirectoryScroll = (m_DirectoryScroll > visible)
			? (m_DirectoryScroll - visible) : 0;
	}
	else
	{
		m_DirectoryScroll += visible;

		if (m_DirectoryScroll > maximum)
			m_DirectoryScroll = maximum;
	}

	m_DirectorySelected = m_DirectoryScroll;
	EnsureDirectoryVisible();
}

void InputUi::UpdateDirectory()
{
	if (m_DirectoryDetail)
	{
		if (CancelButtonPressed() || AcceptButtonPressed())
			m_DirectoryDetail = false;

		return;
	}

	if (m_DirectoryEditing)
	{
		if (CancelButtonPressed())
		{
			CancelDirectoryEdit();
			return;
		}

		if (AcceptButtonPressed())
		{
			ApplyDirectoryEdit();
			return;
		}

		UpdateTextGridMovement(TEXT_COLUMNS, TEXT_ROWS, true);
		TypeTextGridCharacter(TEXT_LAYOUT, TEXT_COLUMNS, true);
		return;
	}

	if (CancelButtonPressed())
	{
		ClearDirectory();
		m_Mode = Mode_None;
		return;
	}

	if (m_DirectoryLoadResult != GameList_Loaded)
	{
		if (AcceptButtonPressed())
		{
			ClearDirectory();
			m_Mode = Mode_None;
		}

		return;
	}

	if (m_DirectoryRows.empty())
		m_DirectorySearchFocused = true;

	if (g_Input.IsButtonDownFor(Input::BUTTON_TRIANGLE, 250))
	{
		const bool searchFocused = m_DirectorySearchFocused;

		m_DirectoryGrouped = !m_DirectoryGrouped;
		m_DirectorySelected = 0;
		m_DirectoryScroll = 0;
		RebuildDirectoryRows();
		m_DirectorySearchFocused = searchFocused;

		if (m_DirectoryRows.empty())
			m_DirectorySearchFocused = true;
	}

	if (!m_DirectorySearchFocused)
	{
		if (g_Input.IsButtonDownFor(Input::BUTTON_L1, 250))
			PageDirectory(-1);

		if (g_Input.IsButtonDownFor(Input::BUTTON_R1, 250))
			PageDirectory(1);
	}

	if (m_DirectorySearchFocused)
	{
		if (ConfirmButtonPressed())
		{
			BeginDirectoryEdit();
			return;
		}

		if (g_Input.IsButtonDownFor(Input::BUTTON_PAD_DOWN, 250) && !m_DirectoryRows.empty())
		{
			m_DirectorySearchFocused = false;
			m_DirectorySelected = 0;
			m_DirectoryScroll = 0;
		}

		return;
	}

	if (g_Input.IsButtonDownAccelerated(Input::BUTTON_PAD_UP, 350, 80, 25))
	{
		if (m_DirectorySelected == 0)
			m_DirectorySearchFocused = true;
		else
			MoveDirectorySelection(-1);
	}
	else if (g_Input.IsButtonDownAccelerated(Input::BUTTON_PAD_DOWN, 350, 80, 25))
	{
		MoveDirectorySelection(1);
	}

	if (ConfirmButtonPressed())
	{
		m_DirectoryDetail = true;
		m_DirectoryDetailRow = m_DirectorySelected;
	}
}

void InputUi::DrawDirectorySearchField()
{
	const float height = DIRECTORY_SEARCH_HEIGHT;
	const float width = m_BoxWidth - 60.0f;
	const float centerY = m_BoxMaxY - DIRECTORY_SEARCH_FROM_TOP - (height / 2.0f);
	const bool focused = m_DirectorySearchFocused || m_DirectoryEditing;
	const paf::vec4 border = focused
		? paf::vec4(0.0f, 1.0f, 1.0f, 0.90f)
		: paf::vec4(0.35f, 0.35f, 0.40f, 0.90f);

	g_Renderer.Rectangle(paf::vec2(0.0f, centerY), paf::vec2(width, height),
		border, Renderer::Centered, Renderer::Centered);

	g_Renderer.Rectangle(paf::vec2(0.0f, centerY), paf::vec2(width - 6.0f, height - 6.0f),
		paf::vec4(0.03f, 0.03f, 0.05f, 1.0f), Renderer::Centered, Renderer::Centered);

	std::wstring text = m_DirectoryEditing ? Widen(m_Text) : Widen(m_DirectoryQuery);
	const bool empty = text.empty();

	if (empty)
		text = T(TEXT_DIRECTORY_SEARCH);

	g_Renderer.Text(Truncate(text, 64),
		paf::vec2(m_BoxLeft + 46.0f, centerY), 20.0f,
		empty ? paf::vec4(0.60f, 0.60f, 0.65f, 1.0f) : paf::vec4(1.0f, 1.0f, 1.0f, 1.0f),
		Renderer::Left, Renderer::Centered);
}

void InputUi::DrawDirectoryStatus()
{
	const wchar_t* view = m_DirectoryGrouped
		? T(TEXT_DIRECTORY_VIEW_GAME) : T(TEXT_DIRECTORY_VIEW_PROJECT);

	g_Renderer.Text(view,
		paf::vec2(m_BoxLeft + 44.0f, m_BoxMaxY - DIRECTORY_STATUS_FROM_TOP), 14.0f,
		paf::vec4(0.65f, 0.65f, 0.70f, 1.0f), Renderer::Left, Renderer::Centered);

	if (m_DirectoryRows.empty())
		return;

	const size_t first = m_DirectoryScroll;
	size_t last = m_DirectoryScroll + static_cast<size_t>(DIRECTORY_VISIBLE_ROWS);

	if (last > m_DirectoryRows.size())
		last = m_DirectoryRows.size();

	std::wstring page = to_wstring(static_cast<int>(first + 1));
	page += L"-";
	page += to_wstring(static_cast<int>(last));
	page += L" / ";
	page += to_wstring(static_cast<int>(m_DirectoryRows.size()));

	g_Renderer.Text(page,
		paf::vec2(m_BoxLeft + m_BoxWidth - 44.0f, m_BoxMaxY - DIRECTORY_STATUS_FROM_TOP), 14.0f,
		paf::vec4(0.65f, 0.65f, 0.70f, 1.0f), Renderer::Right, Renderer::Centered);
}

void InputUi::DrawDirectoryList()
{
	if (m_DirectoryRows.empty())
	{
		const wchar_t* message = m_DirectoryQuery.empty()
			? T(TEXT_DIRECTORY_EMPTY) : T(TEXT_DIRECTORY_NO_RESULTS);

		g_Renderer.Text(message,
			paf::vec2(0.0f, 0.0f), 20.0f,
			paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Centered, Renderer::Centered);
		return;
	}

	const float firstCenter = m_BoxMaxY - DIRECTORY_LIST_FROM_TOP - (DIRECTORY_ROW_HEIGHT / 2.0f);
	const size_t visible = static_cast<size_t>(DIRECTORY_VISIBLE_ROWS);
	size_t last = m_DirectoryScroll + visible;

	if (last > m_DirectoryRows.size())
		last = m_DirectoryRows.size();

	bool hasTags = false;

	for (size_t i = m_DirectoryScroll; i < last; i++)
	{
		if (m_DirectoryRows[i].tags != GameTag_None)
		{
			hasTags = true;
			break;
		}
	}

	std::wstring patchLabel;
	std::wstring whitelistLabel;
	std::wstring discordLabel;
	float patchWidth = 0.0f;
	float whitelistWidth = 0.0f;
	float discordWidth = 0.0f;

	if (hasTags)
	{
		patchLabel = kDirectoryTagPatch;
		whitelistLabel = kDirectoryTagWhitelist;
		discordLabel = kDirectoryTagDiscord;
		patchWidth = DirectoryTagWidth(patchLabel);
		whitelistWidth = DirectoryTagWidth(whitelistLabel);
		discordWidth = DirectoryTagWidth(discordLabel);
	}

	const float tagRight = m_BoxLeft + m_BoxWidth - 44.0f;

	for (size_t i = m_DirectoryScroll; i < last; i++)
	{
		const GameDirectoryRow& row = m_DirectoryRows[i];
		const float rowCenter = firstCenter - static_cast<float>(i - m_DirectoryScroll) * DIRECTORY_ROW_HEIGHT;

		if (i == m_DirectorySelected)
		{
			const float alpha = m_DirectorySearchFocused ? 0.18f : 0.35f;

			g_Renderer.Rectangle(paf::vec2(0.0f, rowCenter),
				paf::vec2(m_BoxWidth - 60.0f, DIRECTORY_ROW_HEIGHT - 4.0f),
				paf::vec4(0.0f, 1.0f, 1.0f, alpha),
				Renderer::Centered, Renderer::Centered);
		}

		const std::wstring title = Truncate(row.game, 68);

		g_Renderer.Text(title,
			paf::vec2(m_BoxLeft + 44.0f, rowCenter + 8.0f), 18.0f,
			paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Left, Renderer::Centered);

		if (row.tags != GameTag_None)
		{
			const float tagX = tagRight - DirectoryTagsWidth(row.tags, patchWidth,
				whitelistWidth, discordWidth);

			DrawDirectoryTags(row.tags, tagX, rowCenter + 8.0f,
				patchLabel, patchWidth, whitelistLabel, whitelistWidth,
				discordLabel, discordWidth);
		}

		std::wstring detail = FormatDirectoryDetail(row);

		g_Renderer.Text(Truncate(detail, DIRECTORY_DETAIL_TEXT_LIMIT),
			paf::vec2(m_BoxLeft + 44.0f, rowCenter - 9.0f), 13.0f,
			paf::vec4(0.65f, 0.65f, 0.70f, 1.0f), Renderer::Left, Renderer::Centered);
	}
}

void InputUi::DrawDirectoryDetail()
{
	if (m_DirectoryDetailRow >= m_DirectoryRows.size())
		return;

	const GameDirectoryRow& row = m_DirectoryRows[m_DirectoryDetailRow];

	DrawBox(760.0f, 460.0f);

	g_Renderer.Text(Truncate(row.game, 54),
		paf::vec2(m_BoxLeft + 30.0f, m_BoxMaxY - 30.0f), 24.0f,
		paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Left, Renderer::Centered);

	float y = m_BoxMaxY - 80.0f;

	if (row.tags != GameTag_None)
	{
		const std::wstring patchLabel = kDirectoryTagPatch;
		const std::wstring whitelistLabel = kDirectoryTagWhitelist;
		const std::wstring discordLabel = kDirectoryTagDiscord;
		const float patchWidth = DirectoryTagWidth(patchLabel);
		const float whitelistWidth = DirectoryTagWidth(whitelistLabel);
		const float discordWidth = DirectoryTagWidth(discordLabel);

		DrawDirectoryTags(row.tags, m_BoxLeft + 30.0f, m_BoxMaxY - 60.0f,
			patchLabel, patchWidth, whitelistLabel, whitelistWidth,
			discordLabel, discordWidth);

		y = m_BoxMaxY - 92.0f;
	}

	if (!row.projects.empty())
	{
		g_Renderer.Text(Truncate(Join(row.projects, L", "), 78),
			paf::vec2(m_BoxLeft + 30.0f, y), 16.0f,
			paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Left, Renderer::Centered);

		y -= 24.0f;
	}

	if (!row.ips.empty())
	{
		std::wstring ips;

		for (size_t i = 0; i < row.ips.size(); i++)
		{
			if (!ips.empty())
				ips += L", ";

			ips += Widen(row.ips[i]);
		}

		g_Renderer.Text(Truncate(ips, 78),
			paf::vec2(m_BoxLeft + 30.0f, y), 16.0f,
			paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Left, Renderer::Centered);

		y -= 24.0f;
	}

	if (!row.aliases.empty() && (row.aliases != row.projects) &&
		(y >= m_BoxMinY + 60.0f))
	{
		std::wstring aliases = T(TEXT_DIRECTORY_ALSO_KNOWN_AS);
		aliases += L" ";
		aliases += Join(row.aliases, L", ");

		g_Renderer.Text(Truncate(aliases, 86),
			paf::vec2(m_BoxLeft + 46.0f, y), 13.0f,
			paf::vec4(0.65f, 0.65f, 0.70f, 1.0f), Renderer::Left, Renderer::Centered);

		y -= 20.0f;
	}

	if (!row.notes.empty())
		DrawDirectoryNotes(row, m_BoxLeft + 46.0f, y, m_BoxMinY + 54.0f);

	DrawFooter(T(TEXT_INPUT_INFO_FOOTER), L"");
}

void InputUi::DrawDirectoryError()
{
	const wchar_t* message = T(TEXT_GAMELIST_INVALID);
	const wchar_t* hint = L"";

	if (m_DirectoryLoadResult == GameList_Missing)
	{
		message = T(TEXT_GAMELIST_MISSING_FILE);
		hint = T(TEXT_GAMELIST_MISSING_HINT);
	}
	else if (m_DirectoryLoadResult == GameList_Unreadable)
	{
		message = T(TEXT_GAMELIST_UNREADABLE);
	}

	g_Renderer.Text(message,
		paf::vec2(m_BoxLeft + 40.0f, m_BoxMaxY - 130.0f), 22.0f,
		paf::vec4(1.0f, 1.0f, 1.0f, 1.0f), Renderer::Left, Renderer::Centered);

	if (hint[0] != L'\0')
	{
		g_Renderer.Text(hint,
			paf::vec2(m_BoxLeft + 40.0f, m_BoxMaxY - 164.0f), 16.0f,
			paf::vec4(0.65f, 0.65f, 0.70f, 1.0f), Renderer::Left, Renderer::Centered);
	}
}

void InputUi::DrawDirectory()
{
	if (m_DirectoryDetail)
	{
		if (m_DirectoryDetailRow < m_DirectoryRows.size())
		{
			DrawDirectoryDetail();
			return;
		}

		m_DirectoryDetail = false;
	}

	DrawBox(DIRECTORY_WIDTH, DIRECTORY_HEIGHT);
	DrawTitle();

	if (m_DirectoryLoadResult != GameList_Loaded)
	{
		DrawDirectoryError();
		DrawFooter(T(TEXT_INPUT_INFO_FOOTER), L"");
		return;
	}

	DrawDirectorySearchField();

	if (m_DirectoryEditing)
	{
		DrawCharacterGrid(DIRECTORY_GRID_TOP_FROM_TOP);
		DrawFooter(T(TEXT_INPUT_TEXT_FOOTER_LEFT), T(TEXT_INPUT_TEXT_FOOTER_RIGHT));
		return;
	}

	if (m_DirectoryQuery.size() < static_cast<size_t>(DIRECTORY_MIN_SEARCH_LENGTH))
	{
		DrawFooter(T(TEXT_DIRECTORY_SEARCH_HINT), L"");
		return;
	}

	DrawDirectoryStatus();
	DrawDirectoryList();
	DrawFooter(T(TEXT_DIRECTORY_FOOTER_LEFT), T(TEXT_DIRECTORY_FOOTER_RIGHT));
}

void InputUi::Draw()
{
	if (m_Mode == Mode_None)
		return;

	if (m_Mode == Mode_Directory)
	{
		DrawDirectory();
		return;
	}

	if (m_Mode == Mode_Confirm)
		DrawBox(700.0f, 320.0f);
	else if (m_Mode == Mode_Info)
		DrawBox(760.0f, 460.0f);
	else
		DrawBox(BOX_WIDTH, BOX_HEIGHT);

	DrawTitle();

	if (m_Mode == Mode_Confirm)
	{
		DrawConfirm();
		DrawFooter(T(TEXT_INPUT_CONFIRM_FOOTER), L"");
		return;
	}

	/* Read-only text. Nothing here can be accepted or rejected, so any of the three buttons
	 * closes it rather than pretending there was a choice to make. */
	if (m_Mode == Mode_Info)
	{
		DrawInfo();
		DrawFooter(T(TEXT_INPUT_INFO_FOOTER), L"");
		return;
	}

	DrawValue();

	if (m_Mode == Mode_Numeric)
		DrawNumericPad();
	else
		DrawCharacterGrid(GRID_TOP_FROM_TOP);

	DrawFooter(T(TEXT_INPUT_TEXT_FOOTER_LEFT), T(TEXT_INPUT_TEXT_FOOTER_RIGHT));
}
