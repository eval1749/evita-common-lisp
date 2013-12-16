#include "precomp.h"
//////////////////////////////////////////////////////////////////////////////
//
// evcl - listener - winapp - Text Edit Window
// listener/winapp/vi_TextEditWindow.cpp
//
// Copyright (C) 1996-2007 by Project Vogue.
// Written by Yoshifumi "VOGUE" INOUE. (yosi@msn.com)
//
// @(#)$Id: //proj/evcl3/mainline/listener/winapp/vi_TextEditWindow.cpp#3 $
//
#define DEBUG_AUTOSCROLL 0
#define DEBUG_CARET 0
#define DEBUG_FOCUS 0
#define DEBUG_IDLE 0
#define DEBUG_KEY 0
#define DEBUG_PAINT 0
#define DEBUG_REDRAW 0
#define DEBUG_RESIZE 0
#define DEBUG_SCROLL 0
#define DEBUG_SHOW_HIDE 0
#include "./vi_TextEditWindow.h"

#include "./ed_Style.h"
#include "./gfx_base.h"
#include "./vi_Application.h"
#include "./vi_Buffer.h"
#include "./vi_Caret.h"
#include "./vi_EditPane.h"
#include "./vi_Frame.h"
#include "./vi_Selection.h"
#include "./vi_util.h"

#define DEBUG_TEXT_EDIT_PRINTF(mp_format, ...) \
  DEBUG_PRINTF("|%ls|@%p " mp_format, GetBuffer()->GetName(), this, \
               __VA_ARGS__)

extern HWND g_hwndActiveDialog;

namespace Command {
uint TranslateKey(uint);
}

namespace {

enum MyTimerId {
  MyTimerId_AutoScroll = 1,
  MyTimerId_Blink = 2,
};

static uint s_active_tick;

} // namespace

void TextEditWindow::AutoScroll::Continue(HWND hwnd) {
  #if DEBUG_AUTOSCROLL
    DEBUG_PRINTF("\n");
  #endif // DEBUG_AUTOSCROLL

  m_nTimerId = MyTimerId_AutoScroll;
  ::SetTimer(hwnd, m_nTimerId, 50, nullptr);
}

void TextEditWindow::AutoScroll::Start(HWND hwnd, int iDir) {
  if (!m_nTimerId) {
    m_nStartTick = ::GetTickCount();
    Continue(hwnd);
  }
  m_iDirection = iDir;
}

void TextEditWindow::AutoScroll::Stop(HWND hwnd) {
  if (!m_nTimerId)
    return;

  #if DEBUG_AUTOSCROLL
    DEBUG_PRINTF("id=%d\n", m_nTimterid);
  #endif // DEBUG_AUTOSCROLL

  ::KillTimer(hwnd, m_nTimerId);
  m_nTimerId = 0;
}

void TextEditWindow::ScrollBar::ShowWindow(int code) const {
  if (m_hwnd)
    ::ShowWindow(m_hwnd, code);
}

TextEditWindow::TextEditWindow(void* pvHost, Buffer* pBuffer, Posn lStart)
    : caret_(new Caret()),
      m_eDragMode(DragMode_None),
      m_fBlink(false),
      m_gfx(nullptr),
      m_lCaretPosn(-1),
      m_nActiveTick(0),
      m_nBlinkTimerId(0),
      m_pBlink(pBuffer->CreateRange(0,0)),
      m_pPage(new Page()),
      ALLOW_THIS_IN_INITIALIZER_LIST(selection_(
          new(pBuffer->GetHeap()) Selection(this, pBuffer))),
      m_pViewRange(pBuffer->CreateRange(lStart)),
      #if SUPPORT_IME
        m_fImeTarget(false),
        m_lImeEnd(0),
        m_lImeStart(0),
      #endif // SUPPORT_IME
      m_pvHost(pvHost) {
  pBuffer->AddWindow(this);
}

TextEditWindow::~TextEditWindow() {
  GetSelection()->GetBuffer()->RemoveWindow(this);
}

void TextEditWindow::Activate() {
  #if DEBUG_FOCUS
    DEBUG_TEXT_EDIT_PRINTF("focus=%d show=%d\n", has_focus(), is_shown());
  #endif
  SetFocus();
}

void TextEditWindow::Blink(Posn lPosn, int nMillisecond) {
  ASSERT(nMillisecond >= 0);
  m_pBlink->SetRange(lPosn, lPosn);
  m_fBlink = true;
  Redraw();
  if (!m_nBlinkTimerId) {
    m_nBlinkTimerId = MyTimerId_Blink;
    ::SetTimer(AssociatedHwnd(), m_nBlinkTimerId, 
               static_cast<uint>(nMillisecond), nullptr);
  }
}

Posn TextEditWindow::computeGoalX(float xGoal, Posn lGoal) {
  if (xGoal < 0)
    return lGoal;

  Page::Line* pLine = nullptr;

  if (!m_pPage->IsDirty(rect(), *selection_))
    pLine = m_pPage->FindLine(lGoal);

  if (pLine)
    return pLine->MapXToPosn(*m_gfx, xGoal);

  auto lStart = GetBuffer()->ComputeStartOf(Unit_Paragraph, lGoal);
  Page oPage;
  gfx::RectF page_rect(rect());
  for (;;) {
    auto const pLine = oPage.FormatLine(*m_gfx, page_rect,
                                        *selection_, lStart);
    auto const lEnd = pLine->GetEnd();
    if (lGoal < lEnd)
      return pLine->MapXToPosn(*m_gfx, xGoal);
    lStart = lEnd;
  }
}

Count TextEditWindow::ComputeMotion(Unit eUnit, Count n,
                                    const gfx::PointF& pt,
                                    Posn* inout_lPosn)  {
  switch (eUnit) {
    case Unit_Line:
      if (n > 0) {
        auto const lBufEnd = GetBuffer()->GetEnd();
        auto lGoal = *inout_lPosn;
        auto k = 0;
        for (k = 0; k < n; k++) {
          lGoal = EndOfLine(lGoal);
          if (lGoal >= lBufEnd)
            break;
          lGoal += 1;
        }
        *inout_lPosn = computeGoalX(pt.x, min(lGoal, lBufEnd));
        return k;
      } else if (n < 0) {
        n = -n;

        auto const lBufStart = GetBuffer()->GetStart();
        auto lStart = *inout_lPosn;
        auto k = 0;
        for (k = 0; k < n; k++) {
          lStart = StartOfLine(lStart);
          if (lStart <= lBufStart)
            break;
          lStart -= 1;
        }

        *inout_lPosn = computeGoalX(pt.x, max(lStart, lBufStart));
        return k;
      }
      return 0;

    case Unit_Screen: {
      auto k = LargeScroll(0, n, false);
      if (k > 0) {
        auto const lStart = m_pPage->GetStart();
        m_pViewRange->SetRange(lStart, lStart);
        *inout_lPosn = MapPointToPosn(pt);
      } else if (n > 0) {
        *inout_lPosn = GetEnd();
        k = 1;
      } else if (n < 0) {
        *inout_lPosn = GetStart();
        k = 1;
      }
      return k;
    }

    case Unit_Window:
      if (n > 0) {
        *inout_lPosn = GetEnd();
        return 1;
      }
      if (n < 0) {
        *inout_lPosn = GetStart();
        return 1;
      }
      return 0;

    default:
      return GetBuffer()->ComputeMotion(eUnit, n, inout_lPosn);
  }
}


void TextEditWindow::DidChangeFrame() {
  auto const frame = Frame::FindFrame(*this);
  ASSERT(frame);
  m_gfx = &frame->gfx();
  auto const parent_hwnd = AssociatedHwnd();
  if (auto const hwnd = m_oHoriScrollBar.GetHwnd())
    ::SetParent(hwnd, parent_hwnd);
  if (auto const hwnd = m_oVertScrollBar.GetHwnd())
    ::SetParent(hwnd, parent_hwnd);
}

void TextEditWindow::DidChangeParentWidget() {
  DidChangeFrame();
}

void TextEditWindow::DidHide() {
  // Note: It is OK that hidden window have focus.
  #if DEBUG_SHOW_HIDE
    DEBUG_TEXT_EDIT_PRINTF("show=%d\n", is_shown());
  #endif
  m_oHoriScrollBar.ShowWindow(SW_HIDE);
  m_oVertScrollBar.ShowWindow(SW_HIDE);
}

void TextEditWindow::DidKillFocus() {
  #if DEBUG_FOCUS
    DEBUG_TEXT_EDIT_PRINTF("focus=%d show=%d\n", has_focus(), is_shown());
  #endif
  ParentClass::DidKillFocus();
  caret_->Give(*m_gfx);
}

void TextEditWindow::DidRealize() {
  auto const frame = Frame::FindFrame(*this);
  ASSERT(frame);
  m_gfx = &frame->gfx();
}

void TextEditWindow::DidResize() {
  #if DEBUG_RESIZE
    DEBUG_TEXT_EDIT_PRINTF("focus=%d show=%d " DEBUG_RECT_FORMAT "\n",
        has_focus(), is_shown(), DEBUG_RECT_ARG(rect()));
  #endif
  m_pPage->Reset();
  Redraw();
}

void TextEditWindow::DidSetFocus() {
  #if DEBUG_FOCUS
    DEBUG_TEXT_EDIT_PRINTF("focus=%d show=%d\n", has_focus(), is_shown());
  #endif
  // Note: It is OK to set focus to hidden window.
  caret_->Take(*m_gfx);
  s_active_tick += 1;
  m_nActiveTick = s_active_tick;
  ASSERT(has_focus());
}

void TextEditWindow::DidShow() {
  #if DEBUG_SHOW_HIDE
    DEBUG_TEXT_EDIT_PRINTF("focus=%d show=%d\n", has_focus(), is_shown());
  #endif
  m_oHoriScrollBar.ShowWindow(SW_SHOW);
  m_oVertScrollBar.ShowWindow(SW_SHOW);
}

Posn TextEditWindow::EndOfLine(Posn lPosn) {
  if (!m_pPage->IsDirty(rect(), *selection_)) {
    auto const pLine = m_pPage->FindLine(lPosn);
    if (pLine)
      return pLine->GetEnd() - 1;
  }

  auto const lBufEnd = selection_->GetBuffer()->GetEnd();
  return lPosn >= lBufEnd ? lBufEnd : endOfLineAux(*m_gfx, lPosn);
}

Posn TextEditWindow::endOfLineAux(const gfx::Graphics& gfx, Posn lPosn) {
  auto const lBufEnd = selection_->GetBuffer()->GetEnd();
  if (lPosn >= lBufEnd)
    return lBufEnd;

  Page oPage;
  gfx::RectF page_rect(rect());
  auto lStart = selection_->GetBuffer()->ComputeStartOf(Unit_Paragraph, lPosn);
  for (;;) {
    auto const pLine = oPage.FormatLine(gfx, page_rect, *selection_, lStart);
    lStart = pLine->GetEnd();
    if (lPosn < lStart)
      return lStart - 1;
  }
}

void TextEditWindow::format(const gfx::Graphics& gfx, Posn lStart) {
  m_pPage->Format(gfx, gfx::RectF(rect()), *selection_, lStart);
}

Buffer* TextEditWindow::GetBuffer() const {
  return selection_->GetBuffer();
}

Count TextEditWindow::GetColumn(Posn lPosn) {
  auto const lStart = StartOfLine(lPosn);
  return lPosn - lStart;
}

HCURSOR TextEditWindow::GetCursorAt(const Point&) const {
  #pragma warning(suppress: 4302)
  return ::LoadCursor(nullptr, MAKEINTRESOURCE(IDC_IBEAM));
}


// For Selection.MoveDown Screen
Posn TextEditWindow::GetEnd() {
  updateScreen();
  return m_pPage->GetEnd();
}

HWND TextEditWindow::GetScrollBarHwnd(int nBar) const {
  if (nBar == SB_VERT)
    return m_oVertScrollBar.GetHwnd();
  return nullptr;
}

//For Selection.MoveUp Screen
Posn TextEditWindow::GetStart() {
  updateScreen();
 return m_pPage->GetStart();
}

int TextEditWindow::LargeScroll(int, int iDy, bool fRender) {
  updateScreen();

  if (iDy < 0) {
    // Scroll Down -- place top line out of window.
    iDy = -iDy;

    auto const lBufStart = selection_->GetBuffer()->GetStart();
    int k;
    for (k = 0; k < iDy; k++) {
      auto const lStart = m_pPage->GetStart();
      if (lStart == lBufStart)
        break;

      // Scroll down until page start goes out to page.
      do {
        #if DEBUG_SCROLL
          DEBUG_TEXT_EDIT_PRINTF("scroll down lStart=%d\n", lStart);
        #endif
        if (!m_pPage->ScrollDown(*m_gfx))
          break;
      } while (m_pPage->GetEnd() != lStart);
    }

     if (fRender && k > 0)
       Render();
      return k;
  } else if (iDy > 0) {
    // Scroll Up -- format page from page end.
    const Posn lBufEnd = selection_->GetBuffer()->GetEnd();
    int k;
    for (k = 0; k < iDy; k++) {
      auto const lStart = m_pPage->GetEnd();
      if (lStart >= lBufEnd)
        break;
      #if DEBUG_SCROLL
        DEBUG_TEXT_EDIT_PRINTF("scroll up lStart=%d\n", lStart);
      #endif
      format(*m_gfx, lStart);
    }

    if (fRender && k > 0)
      Render();
    return k;
  }
  return 0;
}

Command::KeyBindEntry* TextEditWindow::MapKey(uint nKey) {
  return GetBuffer()->MapKey(nKey);
}

void TextEditWindow::MakeSelectionVisible() {
  #if DEBUG_CARET
    DEBUG_TEXT_EDIT_PRINTF("[%d,%d]\n",
        this, selection_->GetStart(), selection_->GetEnd());
  #endif

  m_lCaretPosn = -1;
  redraw(true);
}

Posn TextEditWindow::MapPointToPosn(const gfx::PointF pt) {
  updateScreen();
  return m_pPage->MapPointToPosn(*m_gfx, pt);
}

// Description:
// Maps position specified buffer position and returns height
// of caret, If specified buffer position isn't in window, this function
// returns 0.
gfx::RectF TextEditWindow::MapPosnToPoint(Posn lPosn) {
  updateScreen();
  for (;;) {
    if (auto rect = m_pPage->MapPosnToPoint(*m_gfx, lPosn))
      return rect;
    m_pPage->ScrollToPosn(*m_gfx, lPosn);
  }
}

bool TextEditWindow::OnIdle(uint count) {
  auto const more = GetBuffer()->OnIdle(count);

  #if DEBUG_IDLE
    DEBUG_TEXT_EDIT_PRINTF("count=%d more=%d\n", count, more);
  #endif

  gfx::Graphics::DrawingScope drawing_scope(*m_gfx);
  Redraw();
  return more;
}

void TextEditWindow::OnLeftButtonDown(uint flags, const Point& point) {
  auto const lPosn = MapPointToPosn(point);
  #if DEBUG_FOCUS
    DEBUG_TEXT_EDIT_PRINTF(DEBUG_POINT_FORMAT " focus=%d show=%d p=%d\n",
        DEBUG_POINT_ARG(point),
        has_focus(), is_shown(), lPosn);
  #endif

  if (lPosn < 0) {
    // Click outside window. We do nothing.
    return;
  }

  if (!has_focus()) {
    SetFocus();
    if (lPosn >= GetSelection()->GetStart() &&
        lPosn < GetSelection()->GetEnd()) {
     return;
    }
  }

  GetSelection()->MoveTo(lPosn, flags & MK_SHIFT);

  if (flags & MK_CONTROL) {
    selectWord(lPosn);
  } else {
    m_eDragMode = DragMode_Selection;
    SetCapture();
  }
}

void TextEditWindow::OnLeftButtonUp(uint, const Point&) {
  if (m_eDragMode == DragMode_None)
    return;
  ReleaseCapture();
  stopDrag();
}

LRESULT TextEditWindow::OnMessage(uint uMsg, WPARAM wParam, LPARAM lParam) {
  if (WM_SYSTIMER == uMsg) { // WM_SYSTIMER for blinking caret
    DEBUG_TEXT_EDIT_PRINTF("WM_SYSTIMER\n");
  }

  switch (uMsg) {
    case WM_CHAR: {
      // Ctrl+<key> is handled by WM_KEYDOWN.
      char16 wch = static_cast<char16>(wParam);
      if (wch >= 0x20) {
        m_fBlink = false;
        Application::Get()->Execute(
            this, wch,  static_cast<uint>(HIWORD(lParam) & KF_REPEAT));
      }
      break;
    }

    case WM_DESTROY:
      #if DEBUG_DESTROY
        DEBUG_TEXT_EDIT_PRINTF("WM_DESTROY\n");
      #endif
      m_nActiveTick = 0;
      break;

    case WM_APPCOMMAND:
      #if DEBUG_KEY
          DEBUG_TEXT_EDIT_PRINTF("WM_APPCOMMAND %x\n", lParam);
      #endif
      return TRUE;

    case WM_KEYDOWN: {
      #if DEBUG_KEY
        DEBUG_TEXT_EDIT_PRINTF("WM_KEYDOWN VKey=0x%0X 0x%04X 0x%04X\r\n",
            wParam, HIWORD(lParam), LOWORD(lParam));
      #endif

      auto const nVKey = static_cast<uint>(wParam);
      auto const nKey = Command::TranslateKey(nVKey);
      if (nKey) {
        m_fBlink = false;
        Application::Get()->Execute(
            this, nKey, static_cast<uint>(HIWORD(lParam) & KF_REPEAT));
        return 0;
      }
      break;
    }

    case WM_LBUTTONDBLCLK: {
      Point pt(MAKEPOINTS(lParam));
      auto const lPosn = MapPointToPosn(pt);
      if (lPosn >= 0)
        selectWord(lPosn);
      return 0;
    }

#if 0
    case WM_LBUTTONDOWN:
      OnLeftButtonDown(static_cast<uint>(wParam), MAKEPOINTS(lParam));
      return 0;

    case WM_LBUTTONUP:
      OnLeftButtonUp(static_cast<uint>(wParam), MAKEPOINTS(lParam));
      return 0;

    case WM_MOUSEMOVE:
      OnMouseMove(static_cast<uint>(wParam), MAKEPOINTS(lParam));
      return 0;
#endif

    case WM_MOUSEWHEEL:
      SmallScroll(0, GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? -2 : 2);
      return 0;

    case WM_USER: {
      gfx::Graphics::DrawingScope drawing_scope(*m_gfx);
      m_pPage->Reset();
      m_pPage->Render(*m_gfx);
      return 0;
    }

    case WM_SIZE:
      #if DEBUG_RESIZE
        DEBUG_TEXT_EDIT_PRINTF("WM_SIZE %dx%d\n",
            LOWORD(lParam), HIWORD(lParam));
      #endif
      return 0;

    case WM_TIMER: {
      #if DEBUG_AUTOSCROLL
      DEBUG_TEXT_EDIT_PRINTF("WM_TIMER: %d dir=%d\n",
          wParam, m_oAutoScroll.m_iDirection);
      #endif // DEBUG_AUTOSCROLL

      switch (wParam) {
        case MyTimerId_AutoScroll:
          if (m_oAutoScroll.m_nTimerId) {
              auto const iDuration = static_cast<Count>(
                  ::GetTickCount() - m_oAutoScroll.m_nStartTick);
              auto lCount = iDuration / 500;
              lCount = max(lCount, 1);
              lCount = min(lCount, 20);

              Count k;
              if (m_oAutoScroll.m_iDirection > 0) {
                  k = selection_->MoveDown(Unit_Line, lCount, true);
              } else {
                  k = selection_->MoveUp(Unit_Line, lCount, true);
              }

              if (!k) {
                m_oAutoScroll.Stop(AssociatedHwnd());
              } else {
                m_oAutoScroll.Continue(AssociatedHwnd());
              }
          }
          break;

        case MyTimerId_Blink:
          ::KillTimer(AssociatedHwnd(), wParam);
          m_nBlinkTimerId = 0;
          m_fBlink = false;
          break;
      }
      break;
    }

    case WM_VSCROLL:
      onVScroll(LOWORD(wParam));
      return 0;

    #if SUPPORT_IME
    case WM_IME_COMPOSITION:
      onImeComposition(lParam);
      return 0;

    case WM_IME_ENDCOMPOSITION:
      m_fImeTarget = false;
      return 0;

    case WM_IME_REQUEST:
      if (IMR_RECONVERTSTRING == wParam) {
          return static_cast<LRESULT>(setReconvert(
              reinterpret_cast<RECONVERTSTRING*>(lParam),
              GetSelection()->GetStart(),
              GetSelection()->GetEnd()));
      }
      break;

    case WM_IME_SETCONTEXT:
      // We draw composition string instead of IME. So, we don't
      // need default composition window.
      lParam &= ~ISC_SHOWUICOMPOSITIONWINDOW;
      break;

    case WM_IME_STARTCOMPOSITION:
      if (!m_fImeTarget) {
        m_lImeStart = GetSelection()->GetStart();
        m_lImeEnd = m_lImeStart;
        m_fImeTarget = false;
      }
      return 0;
    #endif // SUPPORT_IME
  }
  return ParentClass::OnMessage(uMsg, wParam, lParam);
}

void TextEditWindow::OnMouseMove(uint, const Point& point) {
  if (m_eDragMode == DragMode_None) {
    // We have nothing to do if mouse isn't dragged.
    return;
  }

  if (::GetCapture() != AssociatedHwnd()){
    // Someone takes mouse capture.
    stopDrag();
    return;
  }

  auto const lPosn = MapPointToPosn(point);
  if (lPosn >= 0)
    selection_->MoveTo(lPosn, true);

  #if DEBUG_FORMAT
    DEBUG_TEXT_EDIT_PRINTF("WM_MOUSEMOVE: " DEBUG_POINT_FORMAT "\n",
        DEBUG_POINT_ARG);
  #endif // DEBUG_FORMAT

  if (point.y < rect().top)
    m_oAutoScroll.Start(AssociatedHwnd(), -1);
  else if (point.y > rect().bottom)
    m_oAutoScroll.Start(AssociatedHwnd(), 1);
  else
    m_oAutoScroll.Stop(AssociatedHwnd());
}

void TextEditWindow::OnPaint(const gfx::Rect) {
  m_pPage->Reset();
  Redraw();
}

void TextEditWindow::onVScroll(uint nCode) {
  switch (nCode) {
    case SB_ENDSCROLL: // 8
      break;

    case SB_LINEDOWN: // 1
      SmallScroll(0, 1);
      break;

    case SB_LINEUP: // 0
      SmallScroll(0, -1);
      break;

    case SB_PAGEDOWN: // 3
      LargeScroll(0, 1);
      break;

    case SB_PAGEUP: // 2
      LargeScroll(0, -1);
      break;

    case SB_THUMBPOSITION: // 4
      return;

    case SB_THUMBTRACK: { // 5
      SCROLLINFO oInfo;
      oInfo.cbSize = sizeof(oInfo);
      oInfo.fMask = SIF_ALL;
      if (m_oVertScrollBar.GetInfo(&oInfo)) {
        auto const lStart = startOfLineAux(*m_gfx, oInfo.nTrackPos);
        format(*m_gfx, lStart);
        Render();
      }
      break;
    }

    default:
      return;
  }
}

void TextEditWindow::Redraw() {
  auto fSelectionActive = has_focus();

  if (g_hwndActiveDialog) {
    auto const edit_pane = Application::Get()->GetActiveFrame()->
        GetActivePane()->DynamicCast<EditPane>();
    if (edit_pane)
      fSelectionActive = edit_pane->GetActiveWindow() == this;
  }
  redraw(fSelectionActive);
}

void TextEditWindow::redraw(bool fSelectionIsActive) {
  #if DEBUG_REDRAW
    DEBUG_TEXT_EDIT_PRINTF(DEBUG_RECT_FORMAT
                           " ~~~~~~~~~~Start selection_is_active=%d\n",
        DEBUG_RECT_ARG(rect()), fSelectionIsActive);
  #endif
  Posn lCaretPosn;
  Posn lSelStart;
  Posn lSelEnd;

  if (m_fBlink) {
    lSelStart = m_pBlink->GetStart();
    lSelEnd = m_pBlink->GetEnd();
  } else {
    lSelStart = selection_->GetStart();
    lSelEnd = selection_->GetEnd();
  }

  if (fSelectionIsActive) {
    lCaretPosn = selection_->IsStartActive() ? lSelStart : lSelEnd;

    // EvEdit 1.0's highlight color
    //selection_->SetColor(Color(0, 0, 0));
    //selection_->SetBackground(Color(0xCC, 0xCC, 0xFF));

    // We should not use GetSysColor. If we want to use here
    // default background must be obtained from GetSysColor.
    //selection_->SetColor(::GetSysColor(COLOR_HIGHLIGHTTEXT));
    //selection_->SetBackground(::GetSysColor(COLOR_HIGHLIGHT));

    // We use Vista's highlight color.
    selection_->SetColor(Color(255, 255, 255));
    selection_->SetBackground(Color(51, 153, 255));
  } else {
    lCaretPosn = m_lCaretPosn;

    Posn lEnd = GetBuffer()->GetEnd();
    if (lSelStart == lEnd && lSelEnd == lEnd)
      lCaretPosn = lEnd;

     selection_->SetColor(Color(67, 78, 84));
     selection_->SetBackground(Color(191, 205, 219));
  }

  {
    auto const lStart = m_pViewRange->GetStart();
    if (m_pPage->IsDirty(rect(), *selection_, fSelectionIsActive)) {
      #if DEBUG_REDRAW
        DEBUG_TEXT_EDIT_PRINTF("Page %p is dirty. lStart=%d\n",
            m_pPage, lStart);
      #endif // DEBUG_REDRAW
      format(*m_gfx, startOfLineAux(*m_gfx, lStart));

      if (m_lCaretPosn != lCaretPosn) {
        // FIXME 2007-05-12 Fill the page with lines.
        #if DEBUG_REDRAW
          DEBUG_TEXT_EDIT_PRINTF("ScrollToPosn %d\n", lCaretPosn);
        #endif
        m_pPage->ScrollToPosn(*m_gfx, lCaretPosn);
        m_lCaretPosn = lCaretPosn;
      }
    } else if (m_lCaretPosn != lCaretPosn) {
        m_pPage->ScrollToPosn(*m_gfx, lCaretPosn);
        m_lCaretPosn = lCaretPosn;
    } else if (m_pPage->GetStart() != lStart) {
        #if DEBUG_REDRAW
        DEBUG_TEXT_EDIT_PRINTF("Page %p change start %d to %d\n",
            m_pPage, m_pPage->GetStart(), lStart);
        #endif // DEBUG_REDRAW
        format(*m_gfx, startOfLineAux(*m_gfx, lStart));
    }
  }

  Render();

  #if DEBUG_REDRAW
    DEBUG_TEXT_EDIT_PRINTF(DEBUG_RECT_FORMAT
                           " ~~~~~~~~~~ End Page=[%d,%d]\n",
        DEBUG_RECT_ARG(rect()), m_pPage->GetStart(), m_pPage->GetEnd());
  #endif
}

void TextEditWindow::Render() {
  if (!is_shown())
    return;

  gfx::Graphics::DrawingScope drawing_scope(*m_gfx);
  caret_->Hide(*m_gfx);
  m_pPage->Render(*m_gfx);

  {
    auto const lStart = m_pPage->GetStart();
    m_pViewRange->SetRange(lStart, lStart);
    updateScrollBar();
  }

  if (!has_focus())
    return;

  const auto rect = m_pPage->MapPosnToPoint(*m_gfx, m_lCaretPosn);
  if (!rect)
    return;

  auto const caret_width = max(::GetSystemMetrics(SM_CXBORDER), 2.0f);
  gfx::RectF caret_rect(rect.left_top(),
                        gfx::SizeF(caret_width,  rect.height()));
  #if SUPPORT_IME
    if (m_fImeTarget) {
      POINT pt = {
        static_cast<int>(rect.left),
        static_cast<int>(rect.top)
      };

      SIZE size = {
        static_cast<int>(rect.width()),
        static_cast<int>(rect.height())
      };

      if (showImeCaret(size, pt))
        return;
      DEBUG_TEXT_EDIT_PRINTF("showImeCaret failed.\r\n");
    }
  #endif // SUPPORT_IME

  caret_->Show(*m_gfx, caret_rect);
}

void TextEditWindow::selectWord(Posn lPosn) {
  auto const pSelection = GetSelection();
  pSelection->SetStart(lPosn);
  pSelection->StartOf(Unit_Word);
  pSelection->EndOf(Unit_Word, true);
  pSelection->SetStartIsActive(false);
}

void TextEditWindow::SetScrollBar(HWND hwnd, int nBar) {
  ASSERT(nBar == SB_VERT)
  m_oVertScrollBar.Set(hwnd, SB_CTL);
}

int TextEditWindow::SmallScroll(int, int iDy) {
  updateScreen();

  if (iDy < 0) {
    iDy = -iDy;

    auto const lBufStart = selection_->GetBuffer()->GetStart();
    auto lStart = m_pPage->GetStart();
    int k;
    for (k = 0; k < iDy; k++) {
      if (lStart == lBufStart)
        break;
        lStart = startOfLineAux(*m_gfx, lStart - 1);
    }

    if (k > 0) {
      #if DEBUG_FORMAT
        DEBUG_TEXT_EDIT_PRINTF("down lStart=%d\n", lStart);
      #endif // DEBUG_FORMAT
      format(*m_gfx, lStart);
      Render();
    }
    return k;
  }

  if (iDy > 0) {
    auto const lBufEnd = selection_->GetBuffer()->GetEnd();
    int k;
    for (k = 0; k < iDy; k++) {
      if (m_pPage->GetEnd() >= lBufEnd) {
          // Make sure whole line of buffer end is visible.
          m_pPage->ScrollToPosn(*m_gfx, lBufEnd);
          k += 1;
          break;
      }

      if (!m_pPage->ScrollUp(*m_gfx))
        break;
    }

    if (k > 0)
        Render();
    return k;
  }

  return 0;
}

Posn TextEditWindow::StartOfLine(Posn lPosn) {
  return lPosn <= 0 ? 0 : startOfLineAux(*m_gfx, lPosn);
}

// See Also:
// EditPange::endOfLineAux
// Description:
// Returns start position of window line of specified position.
Posn TextEditWindow::startOfLineAux(const gfx::Graphics& gfx, Posn lPosn) {
  if (!m_pPage->IsDirty(rect(), *selection_)) {
    auto const pLine = m_pPage->FindLine(lPosn);
    if (pLine)
      return pLine->GetStart();
  }

  auto lStart = selection_->GetBuffer()->ComputeStartOf(
      Unit_Paragraph,
      lPosn);
  if (!lStart)
    return 0;

  Page oPage;
  gfx::RectF page_rect(rect());
  for (;;) {
    auto const pLine = oPage.FormatLine(gfx, page_rect, *selection_,
                                        lStart);

    auto const lEnd = pLine->GetEnd();
    if (lPosn < lEnd)
      return pLine->GetStart();
    lStart = lEnd;
  }
}

void TextEditWindow::stopDrag() {
  m_oAutoScroll.Stop(AssociatedHwnd());
 m_eDragMode = DragMode_None;
}

void TextEditWindow::updateScreen() {
  if (!m_pPage->IsDirty(rect(), *selection_))
    return;

  Posn lStart = m_pViewRange->GetStart();
  lStart = startOfLineAux(*m_gfx, lStart);
  #if DEBUG_REDRAW
    DEBUG_TEXT_EDIT_PRINTF("dirty page lStart=%d\n", lStart);
  #endif // DEBUG_REDRAW
  format(*m_gfx, lStart);
}

void TextEditWindow::updateScrollBar() {
  if (!m_pPage->GetBuffer())
    return;

  auto const lBufEnd = m_pPage->GetBuffer()->GetEnd() + 1;

  SCROLLINFO oInfo;
  oInfo.cbSize = sizeof(oInfo);
  oInfo.fMask = SIF_POS | SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
  oInfo.nPage = static_cast<uint>(m_pPage->GetEnd() - m_pPage->GetStart());
  oInfo.nMin = 0;
  oInfo.nMax = lBufEnd;
  oInfo.nPos = m_pPage->GetStart();

  if (static_cast<Count>(oInfo.nPage) >= lBufEnd) {
    // Current screen shows entire buffer. We disable scroll bar.
    oInfo.nMax = 0;
  }

  m_oVertScrollBar.SetInfo(&oInfo, true);
}

#if SUPPORT_IME

// See Also Caret::Show for moving candidate window.

#include <imm.h>
#pragma comment(lib, "imm32.lib")

extern StyleValues g_DefaultStyle;
StyleValues g_rgpImeStyleConverted[2];
StyleValues g_pImeStyleInput;
StyleValues g_pImeStyleTargetConverted;
StyleValues g_pImeStyleTargetNotConverted;

#define GCS_COMPSTRATTR (GCS_COMPSTR | GCS_COMPATTR | GCS_CURSORPOS)

class Imc {
  private: HWND m_hwnd;
  private: HIMC m_himc;

  public: Imc(HWND hwnd) : m_hwnd(hwnd), m_himc(::ImmGetContext(hwnd)) {}

  public: ~Imc() {
    if (m_himc)
      ::ImmReleaseContext(m_hwnd, m_himc);
  }

  public: operator HIMC() const { return m_himc; }
};

void TextEditWindow::onImeComposition(LPARAM lParam) {
  Imc imc(AssociatedHwnd());
  if (!imc)
    return;

  Edit::UndoBlock oUndo(GetSelection(), L"IME");

  char16 rgwch[1024];
  // If IME has result string, we can insert it into buffer.
  if (lParam & GCS_RESULTSTR) {
    // Remove previous composition string. If user inputs "Space",
    // IME set GCS_RESULTSTR without composition.
    if (m_lImeStart != m_lImeEnd) {
      GetSelection()->SetRange(m_lImeStart, m_lImeEnd);
      GetSelection()->SetText(L"", 0);
    }

    // Get result string
    auto const cwch = ::ImmGetCompositionString(
        imc, GCS_RESULTSTR, rgwch, sizeof(rgwch)) / sizeof(char16);

    // Insert result string into buffer
    if (cwch >= 1) {
      GetSelection()->SetText(rgwch, static_cast<int>(cwch));
      GetSelection()->Collapse(Collapse_End);
      m_lImeEnd = GetSelection()->GetEnd();
      m_lImeStart = m_lImeEnd;
    }
  }

  // IME has composition string
  if ((lParam & GCS_COMPSTRATTR) == GCS_COMPSTRATTR) {
    // Remove previous composition string
    if (m_lImeStart != m_lImeEnd) {
        GetSelection()->SetRange(m_lImeStart, m_lImeEnd);
        GetSelection()->SetText(L"", 0);
        m_lImeEnd = m_lImeStart;
    }

    // Get composition string
    auto const cwch = ::ImmGetCompositionString(
        imc, GCS_COMPSTR, rgwch, sizeof(rgwch)) / sizeof(char16);

    // Get composition attributes
    char rgbAttr[lengthof(rgwch)];
    auto const cbAttr = ::ImmGetCompositionString(
        imc, GCS_COMPATTR, rgbAttr, sizeof(rgbAttr));
    if (cbAttr != static_cast<int>(cwch)) {
      DEBUG_TEXT_EDIT_PRINTF("GCCS_COMPATTR\n");
      return;
    }

    auto const lCursor = ::ImmGetCompositionString(
          imc, GCS_CURSORPOS, nullptr, 0);
    if (lCursor < 0) {
      DEBUG_TEXT_EDIT_PRINTF("GCCS_CURSORPOS\n");
      return;
    }

    uint32 rgnClause[100];
    ::ImmGetCompositionString(imc, GCS_COMPCLAUSE, rgnClause,
                              sizeof(rgnClause));

    GetSelection()->SetText(rgwch, static_cast<int>(cwch));
    GetSelection()->Collapse(Collapse_End);
    m_lImeEnd = GetSelection()->GetEnd();
    GetSelection()->SetRange(m_lImeStart + lCursor, m_lImeStart + lCursor);

    if (!g_pImeStyleInput.m_rgfMask) {
          // Converted[0]
          g_rgpImeStyleConverted[0].m_rgfMask =
              StyleValues::Mask_Decoration;

          g_rgpImeStyleConverted[0].m_eDecoration =
              TextDecoration_ImeInactiveA;

          // Converted[1]
          g_rgpImeStyleConverted[1].m_rgfMask =
              StyleValues::Mask_Decoration;

          g_rgpImeStyleConverted[1].m_eDecoration =
              TextDecoration_ImeInactiveB;

          // Input
          g_pImeStyleInput.m_rgfMask =
              StyleValues::Mask_Decoration;

          g_pImeStyleInput.m_eDecoration =
              TextDecoration_ImeInput;

          // Target Converted
          g_pImeStyleTargetConverted.m_rgfMask =
              StyleValues::Mask_Decoration;

          g_pImeStyleTargetConverted.m_eDecoration =
              TextDecoration_ImeActive;

          // Target Not Converted
          g_pImeStyleTargetNotConverted.m_rgfMask =
              StyleValues::Mask_Background |
              StyleValues::Mask_Color |
              StyleValues::Mask_Decoration;

          #if 0
              g_pImeStyleTargetNotConverted.m_crBackground =
                  g_DefaultStyle.GetColor();

              g_pImeStyleTargetNotConverted.m_crColor =
                  g_DefaultStyle.GetBackground();
          #else
              g_pImeStyleTargetNotConverted.m_crBackground =
                  GetSelection()->GetBackground();

              g_pImeStyleTargetNotConverted.m_crColor =
                  GetSelection()->GetColor();
          #endif

          g_pImeStyleTargetNotConverted.m_eDecoration =
              TextDecoration_None;
      }

      m_fImeTarget = false;
      Posn lEnd = static_cast<Posn>(m_lImeStart + cwch);
      Posn lPosn = m_lImeStart;
      int iClause = 0;
      int iConverted = 0;
      while (lPosn < lEnd) {
        StyleValues* pStyle;
        switch (rgbAttr[lPosn - m_lImeStart]) {
          case ATTR_INPUT:
          case ATTR_INPUT_ERROR:
            pStyle = &g_pImeStyleInput;
            iConverted = 0;
            break;

          case ATTR_TARGET_CONVERTED:
            pStyle = &g_pImeStyleTargetConverted;
            m_fImeTarget = true;
            iConverted = 0;
            break;

          case ATTR_TARGET_NOTCONVERTED:
            pStyle = &g_pImeStyleTargetNotConverted;
            m_fImeTarget = true;
            iConverted = 0;
            break;

          case ATTR_CONVERTED:
            pStyle = &g_rgpImeStyleConverted[iConverted];
            iConverted = 1 - iConverted;
            break;

          default:
            pStyle = &g_pImeStyleInput;
            break;
        }

        iClause += 1;
        Posn lNext = static_cast<Posn>(m_lImeStart + rgnClause[iClause]);
        GetBuffer()->SetStyle(lPosn, lNext, pStyle);
        lPosn = lNext;
      }
  }

  ////////////////////////////////////////////////////////////
  //
  // We have already insert composed string. So, we don't
  // need WM_IME_CHAR and WM_CHAR messages to insert
  // composed string.
  if (lParam & GCS_RESULTSTR)
  {
      m_fImeTarget = false;
      return;
  }

  // Composition was canceled.
  if (0 == lParam)
  {
      m_fImeTarget = false;

      // Remove previous composition string
      GetSelection()->SetRange(m_lImeStart, m_lImeEnd);
      GetSelection()->SetText(L"", 0);

      // if (m_fCancelButLeave)
      {
          auto const cwch = ::ImmGetCompositionString(
              imc,
              GCS_COMPSTR,
              rgwch,
              sizeof(rgwch)) / sizeof(char16);
          if (cwch >= 1)
          {
              GetSelection()->SetText(rgwch, static_cast<int>(cwch));
          }
      }

      m_lImeEnd = m_lImeStart;
  }
}

// Note:
// o IME2000 ignores string after newline.
// o We should limit number of characters to be reconverted.
//
void TextEditWindow::Reconvert(Posn lStart, Posn lEnd) {
  BOOL fSucceeded;

  auto const cb = setReconvert(nullptr, lStart, lEnd);
  if (!cb)
    return;

  auto const pb = new char[cb];
  if (!pb)
    return;

  auto const p = reinterpret_cast<RECONVERTSTRING*>(pb);
  setReconvert(p, lStart, lEnd);

  Imc imc(AssociatedHwnd());
  fSucceeded = ::ImmSetCompositionString(
      imc,
      SCS_QUERYRECONVERTSTRING,
      p,
      cb,
      nullptr,
      0);
  if (!fSucceeded) {
      DEBUG_TEXT_EDIT_PRINTF("SCS_QUERYRECONVERTSTRING\n");
      goto exit;
  }

  m_lImeStart = static_cast<Posn>(lStart + p->dwCompStrOffset / 2);
  m_lImeEnd = static_cast<Posn>(m_lImeStart + p->dwCompStrLen);
  m_fImeTarget = true;

  fSucceeded = ::ImmSetCompositionString(
      imc,
      SCS_SETRECONVERTSTRING,
      p,
      cb,
      nullptr,
      0);
  if (!fSucceeded) {
    DEBUG_TEXT_EDIT_PRINTF("SCS_SETRECONVERTSTRING\n");
    goto exit;
  }

  exit:
    delete[] pb;
}

uint TextEditWindow::setReconvert(RECONVERTSTRING* p, Posn lStart,
                                  Posn lEnd) {
  ASSERT(lEnd >= lStart);
  auto const cwch = lEnd - lStart;
  if (!p || !cwch)
    return 0;

  auto const cb = sizeof(RECONVERTSTRING) + sizeof(char16) * (cwch + 1);
  p->dwSize = cb;
  p->dwVersion = 0;
  p->dwStrLen = static_cast<DWORD>(cwch);
  p->dwStrOffset = sizeof(RECONVERTSTRING);
  p->dwCompStrLen = static_cast<DWORD>(cwch); // # of characters
  p->dwCompStrOffset = 0; // byte offset
  p->dwTargetStrLen = p->dwCompStrLen;
  p->dwTargetStrOffset = p->dwCompStrOffset;

  auto const pwch = reinterpret_cast<char16*>(
      reinterpret_cast<char*>(p) + p->dwStrOffset);
  GetBuffer()->GetText(pwch, lStart, lEnd);
  pwch[cwch] = 0;
  return cb;
}

// Set left top coordinate of IME candiate window.
BOOL TextEditWindow::showImeCaret(SIZE sz, POINT pt) {
  Imc imc(AssociatedHwnd());
  if (!imc)
    return FALSE;

  CANDIDATEFORM oCF;
  oCF.dwIndex = 0;
  oCF.dwStyle = CFS_EXCLUDE;
  oCF.ptCurrentPos.x = pt.x;
  oCF.ptCurrentPos.y = pt.y + sz.cy;
  oCF.rcArea.left = pt.x;
  oCF.rcArea.top = pt.y;
  oCF.rcArea.right = pt.x;
  oCF.rcArea.bottom = pt.y + sz.cy;

  return ::ImmSetCandidateWindow(imc, &oCF);
}

#endif // SUPPORT_IME
