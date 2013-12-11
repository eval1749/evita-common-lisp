//////////////////////////////////////////////////////////////////////////////
//
// evcl - listener - edit buffer
// listener/winapp/ed_buffer.h
//
// Copyright (C) 1996-2007 by Project Vogue.
// Written by Yoshifumi "VOGUE" INOUE. (yosi@msn.com)
//
// @(#)$Id: //proj/evcl3/mainline/listener/winapp/vi_Style.h#2 $
//
#if !defined(INCLUDE_listener_winapp_visual_fontset_h)
#define INCLUDE_listener_winapp_visual_fontset_h

#include "./vi_defs.h"

#include "./ed_Style.h"
#include "./li_util.h"

#include <d2d1.h>

namespace gfx {
class Brush;
class FontFace;
class Graphics;
template<typename base, typename element> class Size_;
template<typename base, typename size> class Point_;
template<typename base, typename point, typename size> class Rect_;
typedef Size_<D2D1_SIZE_F, float> SizeF;
typedef Point_<D2D1_POINT_2F, SizeF> PointF;
typedef Rect_<D2D1_RECT_F, PointF, SizeF> RectF;
};

//////////////////////////////////////////////////////////////////////
//
// Font class
//
class Font {
    public: typedef LOGFONT Key;

    private: class FontImpl;
    private: struct SimpleMetrics {
      float ascent;
      float descent;
      float height;
      float fixed_width;
    };

    private: LOGFONT m_oLogFont;
    private: const base::OwnPtr<FontImpl> font_impl_;
    private: const SimpleMetrics metrics_;

    private: Font(const LOGFONT&);
    public: ~Font();

    private: float ascent() const { return metrics_.ascent; }
    public: float descent() const { return metrics_.descent; }
    public: float height() const { return metrics_.height; }

    // [C]
    public: static base::OwnPtr<Font> Create(const LOGFONT*);

    // [D]
    void DrawText(const gfx::Graphics& gfx,const gfx::Brush& text_brush,
                  const gfx::RectF& rect, const char16* chars,
                  uint num_chars) const;

    // [E]
    public: bool EqualKey(const Key* pKey) const {
      return !::memcmp(&m_oLogFont, pKey, sizeof(m_oLogFont));
    }

    // [G]
    public: float GetCharWidth(char16) const;
    public: const Key* GetKey() const { return &m_oLogFont; }
    public: float GetTextWidth(const char16* pwch, uint cwch) const;

    // [H]
    public: bool HasCharacter(char16) const;

    public: uint Hash() const {
      return static_cast<uint>(reinterpret_cast<UINT_PTR>(this));
    }

    public: static int HashKey(const Key*);

    DISALLOW_COPY_AND_ASSIGN(Font);
}; // Font


//////////////////////////////////////////////////////////////////////
//
// FontSet class
//
struct Fonts
{
    Font*   m_rgpFont[10];
    int     m_cFonts;
}; // Fonts

class FontSet : public Fonts
{
    public: typedef Fonts Key;

    public: FontSet()
    {
        m_cFonts = 0;
    } // FontSet

    // [A]
    public: void Add(Font*);

    // [E]
    public: bool EqualKey(const Key* pFonts) const
    {
        if (pFonts->m_cFonts != m_cFonts) return false;
        ASSERT(m_cFonts < lengthof(m_rgpFont));
        return 0 == ::memcmp(
            pFonts->m_rgpFont,
            m_rgpFont,
            sizeof(m_rgpFont[0]) * m_cFonts );
    } // EqualKey

    // [F]
    public: Font* FindFont(const gfx::Graphics&, char16 wch) const {
      return FindFont(wch);
    }

    public: Font* FindFont(char16) const;

    // [G]
    public: static FontSet* Get(const gfx::Graphics&, const StyleValues* p) {
      return Get(p);
    }
    public: static FontSet* Get(const StyleValues*);
    public: const Key* GetKey() const { return this; }

    // [H]
    public: static int HashKey(const Key*);

    // [E]
    public: class EnumFont
    {
        Font**  m_pRunner;
        Font**  m_pEnd;

        public: EnumFont(const FontSet* p) :
            m_pRunner(const_cast<Font**>(&p->m_rgpFont[0])),
            m_pEnd(const_cast<Font**>(&p->m_rgpFont[p->m_cFonts])) {}

        public: bool AtEnd() const { return m_pRunner >= m_pEnd; }
        public: Font* Get() const { ASSERT(! AtEnd()); return *m_pRunner; }
        public: void Next() { ASSERT(! AtEnd()); m_pRunner++; }
    }; // EnumFont
}; // FontSet

#endif //!defined(INCLUDE_listener_winapp_visual_fontset_h)
