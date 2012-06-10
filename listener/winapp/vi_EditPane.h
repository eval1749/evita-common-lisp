//////////////////////////////////////////////////////////////////////////////
//
// evcl - listener - Edit Pane
// listener/winapp/vi_EditPane.h
//
// Copyright (C) 1996-2007 by Project Vogue.
// Written by Yoshifumi "VOGUE" INOUE. (yosi@msn.com)
//
// @(#)$Id: //proj/evcl3/mainline/listener/winapp/vi_EditPane.h#2 $
//
#if !defined(INCLUDE_listener_winapp_visual_EditPane_h)
#define INCLUDE_listener_winapp_visual_EditPane_h

#include "./vi_Pane.h"

class Buffer;
class TextEditWindow;

//////////////////////////////////////////////////////////////////////
//
// EditPane
//
class EditPane : public CommandWindow_<EditPane, Pane>
{
    private: enum Element
    {
        Element_None,

        Element_Splitter,
        Element_SplitterBig,
        Element_VScrollBar,
        Element_Window,
    }; // Element

    private: enum Limits
    {
        k_cySplitter    = 8,
        k_cySplitterBig = 11,

        k_cyMinBox      = k_cySplitter,
    }; // Limits

    private: enum StatusBarPart
    {
        StatusBarPart_Message,
        StatusBarPart_Mode,
        StatusBarPart_CodePage,
        StatusBarPart_Newline,
        StatusBarPart_LineNumber,
        StatusBarPart_Column,
        StatusBarPart_Posn,
        StatusBarPart_Insert,
    }; // StatusBarPart

    private: typedef TextEditWindow Window;
    private: typedef DoubleLinkedList_<Window> Windows;

    ////////////////////////////////////////////////////////////
    //
    // Box
    //  Used for layout boxes
    //
    public: class Box : public DoubleLinkedNode_<Box>
    {
        private: HWND       m_hwndVScrollBar;
        private: Window*    m_pWindow;
        private: RECT       m_rc;

        public: Box(Window* pWindow) :
            m_hwndVScrollBar(NULL),
            m_pWindow(pWindow) {}
            
        public: ~Box();

        // [D]
        public: void DetachWindow() { m_pWindow = NULL; }

        // [G]
        public: RECT*   GetRect()             { return &m_rc; }
        public: HWND    GetVScrollBar() const { return m_hwndVScrollBar; }
        public: Window* GetWindow()     const { return m_pWindow; }

        // [S]
        public: HWND SetVScrollBar(HWND hwnd)
            { return m_hwndVScrollBar = hwnd; }
    }; // Box

    private: typedef DoubleLinkedList_<Box> Boxes;

    ////////////////////////////////////////////////////////////
    //
    // SplitterDrag
    //
    private: class SplitterDrag
    {
        public: enum State
        {
            State_None,

            State_Drag,
            State_DragSingle,
        }; // State

        public: Box*   m_pBox;
        public: State  m_eState;

        public: SplitterDrag() :
            m_eState(State_None),
            m_pBox(NULL) {}

        public: void Start(HWND, State);
        public: void Stop();
    }; // SplitterDrag

    private: enum State
    {
        State_NotRealized,

        State_Destroyed,
        State_Realized,
    }; // State

    private: State          m_eState;
    private: Boxes          m_oBoxes;
    private: SplitterDrag   m_oSplitterDrag;
    private: Windows        m_oWindows;
    private: RECT           m_rc;

    // ctro/dtor
    public:  EditPane(Buffer* pBuffer, Posn = 0);
    public: ~EditPane();

    // [C]
    public: void CloseAllBut(Window*);

    // [D]
    private: void drawSplitters(HDC);
    private: void drawSplitters();

    // [E]
    public: class EnumBox : public Boxes::Enum
    {
        public: EnumBox(const EditPane* pPane) :
            Boxes::Enum(&pPane->m_oBoxes) {}
    }; // EnumBox

    // [G]
    private: Box*    getActiveBox() const;
    public:  Window* GetActiveWindow() const;
    public:  Buffer* GetBuffer() const;

    public: static const char16* GetClass_()
        { return L"EditPane"; }

    public: Window* GetFirstWindow() const
        { return m_oWindows.GetFirst(); }

    public: Window* GetLastWindow() const
        { return m_oWindows.GetLast(); }

    public: override int GetTitle(char16*, int);

    // [H]
    public: override bool HasFocus() const;

    public:  bool HasMultipleWindows() const
        { return m_oWindows.GetFirst() != m_oWindows.GetLast(); }

    private: Element hitTest(POINT, Box**) const;


    // [M]
    public: override Command::KeyBindEntry* MapKey(uint);

    // [O]
    public:  virtual bool    OnIdle(uint);
    private: virtual LRESULT onMessage(UINT, WPARAM, LPARAM);

    // [R]
    private: void realizeBox(Box*);

    // [S]
    private: void    setupStatusBar();
    private: void    setBoxPos(Box*) const;
    public:  Window* SplitVertically();
    public:  Box*    splitVertically(Box*, int);

    // [U]
    public: virtual void UpdateStatusBar();
}; // EditPane

#endif //!defined(INCLUDE_listener_winapp_visual_EditPane_h)