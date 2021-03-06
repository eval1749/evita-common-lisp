//////////////////////////////////////////////////////////////////////////////
//
// evcl - listener - edit buffer
// listener/winapp/ed_buffer.h
//
// Copyright (C) 1996-2007 by Project Vogue.
// Written by Yoshifumi "VOGUE" INOUE. (yosi@msn.com)
//
// @(#)$Id: //proj/evcl3/mainline/listener/winapp/ExactStringMatch.h#1 $
//
#if !defined(INCLUDE_ExactStringMatch_h)
#define INCLUDE_ExactStringMatch_h

#include "./IStringCursor.h"

//////////////////////////////////////////////////////////////////////
//
// ExactStringMatch
//
class ExactStringMatch : public IStringMatcher
{
    private: struct BadCharVec
    {
        int     m_nMaxChar;
        int     m_nMinChar;
        int     m_iShift;
        int*    m_prgi;

        BadCharVec(const SearchParameters* pSearch) :
            m_nMaxChar(0x0000u),
            m_nMinChar(0xFFFFu),
            m_iShift(pSearch->m_cwch + 1),
            m_prgi(NULL)
        {
            if (0 == pSearch->m_cwch)
            {
                return;
            }

            for (int i = 0; i < pSearch->m_cwch; i++)
            {
                char16 wch = pSearch->m_wsz[i];

                if (pSearch->IsIgnoreCase())
                {
                    wch = ::CharUpcase(wch);
                }

                m_nMaxChar = max(m_nMaxChar, wch);
                m_nMinChar = min(m_nMinChar, wch);
            } // for i

            int m = pSearch->m_cwch;

            m_iShift = m + 1;

            m_prgi = new int[m_nMaxChar - m_nMinChar + 1];
            
            if (pSearch->IsBackward())
            {
                m_iShift = -m_iShift;
            }

            for (int i = 0; i <= (m_nMaxChar - m_nMinChar); i++)
            {
                m_prgi[i] = m_iShift;
            } // for i

            if (pSearch->IsBackward())
            {
                for (int i = 0; i < m; i++)
                {
                    char16 wch = pSearch->m_wsz[m - i - 1];

                    if (pSearch->IsIgnoreCase())
                    {
                        wch = ::CharUpcase(wch);
                    }

                    m_prgi[wch - m_nMinChar] = i - m;
                } // for i
            }
            else
            {
                for (int i = 0; i < m; i++)
                {
                    char16 wch = pSearch->m_wsz[i];

                    if (pSearch->IsIgnoreCase())
                    {
                        wch = ::CharUpcase(wch);
                    }

                    m_prgi[wch - m_nMinChar] = m - i;
                } // for i
            }
        } // BadCharVec

        ~BadCharVec()
        {
            delete[] m_prgi;
        } // ~BadCharVec

        int Get(int wch) const
        {
            if (wch < m_nMinChar) return m_iShift;
            if (wch > m_nMaxChar) return m_iShift;
            return m_prgi[wch - m_nMinChar];
        } // Get
    }; // BadCharVec

    private: BadCharVec         m_BadCharVec;
    private: bool               m_fMatched;
    private: long               m_lMatchEnd;
    private: long               m_lMatchStart;
    private: SearchParameters   m_oSearch;
    private: IStringCursor*     m_pICursor;

    // ctor
    public: ExactStringMatch(const SearchParameters*);

    // [F]
    public: virtual bool FirstMatch(IStringCursor*) override;

    // [G]
    public: virtual bool GetMatched(int iIndex, StringRange* out_oRange) override
    {
        if (0 != iIndex) return false;
        if (! m_fMatched) return false;

        out_oRange->m_lStart = m_lMatchStart;
        out_oRange->m_lEnd   = m_lMatchEnd;

        return true;
    } // GetMatch

    private: int getShift(char16 wch) const
    {
        if (m_oSearch.IsIgnoreCase())
        {
            wch = ::CharUpcase(wch);
        }
        return m_BadCharVec.Get(wch);
    } // getShift

    // [N]
    public: virtual bool NextMatch() override;
}; // ExactStringMatch

#endif //!defined(INCLUDE_ExactStringMatch_h)
