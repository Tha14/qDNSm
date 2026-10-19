#pragma once
#undef vector
#include <vector>
#include <vsh/paf.hpp>
#include "Utils/Std.hpp"
#include "Helpers.hpp"

class Renderer
{
public:
	enum Align
	{
		Centered,
		Left = (1 << 0),
		Right = (1 << 1),
		Top = (1 << 4)
	};

	/*
	 * The glyph set a string is drawn with, passed straight through to the text widget.
	 *
	 * The PS3 ships several system faces and the XMB picks between them per string. paf calls this
	 * paf::PhFont::GlyphType, but the vendored paf.hpp declares PhFont nowhere - it appears only in
	 * demangled comments - so there are no named constants to use here and the value is the enum's.
	 *
	 * Default 0 is the face every other string in this module already draws with, so passing
	 * nothing is always correct, and nothing in this module currently passes anything else. A
	 * different face needs the enum recovered first; there is nowhere to put a named constant for
	 * it until then, because inventing a number here would be a guess dressed up as a setting.
	 */
	enum GlyphSet
	{
		GlyphSet_Default = 0
	};

public:

    Renderer() = default;
    ~Renderer();

    void OnUpdate();
    void DestroyWidgets();

    void Rectangle(paf::vec2 position, paf::vec2 size, paf::vec4 color, Align horizontalAlign = Left, Align verticalAlign = Top, const char* systemTexture = nullptr, float angle = 0.0, bool useRadian = false);

    void Text(std::wstring const& text, paf::vec2 position, float height, paf::vec4 color, Align horizontalAlign = Left, Align verticalAlign = Top, float angle = 0.0, bool drawShadow = true, GlyphSet glyphSet = GlyphSet_Default);


private:
    void DestroyUnusedWidgets();
    void HideWidgets();

    void CreateGroups();
    paf::PhText* CreateText(paf::PhWidget* parent = g_Helpers.m_PageAutoOffGuide);
    paf::PhPlane* CreatePlane(paf::PhWidget* parent = g_Helpers.m_PageAutoOffGuide);

    paf::PhText* GetCurrentText();
    paf::PhPlane* GetCurrentPlane();

    void TrimPool(int keepText, int keepPlane);
    void ForgetWidgets();

    void ViewportScale(paf::vec2& vec);
    void ViewportScaleVertical(float& value);

public:
    bool m_CanDraw{};

    float m_ViewportWidth{ 1280.0f };
    float m_ViewportHeight{ 720.0f };

    int m_CurrentText{};
    int m_CurrentPlane{};

    std::vector<paf::PhText*> m_TextNodeChilds{};
    std::vector<paf::PhPlane*> m_PlaneNodeChilds{};

    paf::PhPlane* m_TextNode{};
    paf::PhPlane* m_PlaneNode{};

    paf::PhPlane* m_PlaneOnTop{};

private:
    /*
     * The XMB widget our groups are parented to, as it was when they were created.
     *
     * If the XMB is torn down and rebuilt - which is what happens when a game starts - the
     * old widget and everything under it is gone, and our pointers into that tree are stale.
     * Comparing the pointer values is safe; dereferencing them is not.
     */
    paf::PhWidget* m_Parent{};
};

extern Renderer g_Renderer;