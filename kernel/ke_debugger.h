//////////////////////////////////////////////////////////////////////////////
//
// evcl - kernel - pre-compiled header
// kernel_debugger.h
//
// Copyright (C) 1996-2006 by Project Vogue.
// Written by Yoshifumi "VOGUE" INOUE. (yosi@msn.com)
//
// @(#)$Id: //proj/evcl3/mainline/kernel/ke_debugger.h#4 $
//
#if !defined(INCLUDE_kernel_debugger_h)
#define INCLUDE_kernel_debugger_h

#if ! defined(_DEBUG)
    #define ASSERT(mp_exp)      __assume(mp_exp)
    //#define CAN_NOT_HAPPEN()    __debugbreak()
    #define DEBUG_PRINTF        __noop
    #define REPORT_HRESULT      __noop
    #define REPORT_WIN32_ERROR  (void)
    #define VERIFY(mp_exp)      __assume(mp_exp)
    #define CAN_NOT_HAPPEN() \
        Debugger::Can_Not_Happen(__FILE__, __LINE__, __FUNCTION__)

#else // ! defined(_DEBUG)
    #define ASSERT(mp_exp) \
        Debugger::Assert(__FILE__, __LINE__, __FUNCTION__, #mp_exp, mp_exp)

    #define CAN_NOT_HAPPEN() \
        Debugger::Can_Not_Happen(__FILE__, __LINE__, __FUNCTION__)

    #define DEBUG_PRINTF \
        Debugger::PrintHeader(__FUNCTION__); \
        Debugger::Printf

    #define REPORT_HRESULT(mp_strCallee, mp_hr) \
        Debugger::ReportHRESULT(__FUNCTION__, mp_strCallee, mp_hr)

    #define REPORT_WIN32_ERROR(mp_strCallee, mp_dwError) \
        Debugger::ReportWIN32_ERROR(__FUNCTION__, mp_strCallee, mp_dwError)

    #define VERIFY(mp_exp)   ASSERT(mp_exp)

#endif // ! defined(_DEBUG)


//////////////////////////////////////////////////////////////////////
//
// Compile Time ASSERT
//
#define CASSERT(mp_exp) \
    extern int macro_CASSERT[(mp_exp) ? 1 : -1]

namespace Debugger
{
    void Assert(LPCSTR, int, LPCSTR, LPCSTR, bool);
    void __declspec(noreturn) Can_Not_Happen(LPCSTR, int, LPCSTR);
    void __declspec(noreturn) Fail(LPCWSTR, ...);
    void Printf(LPCWSTR, ...);
    void PrintHeader(LPCSTR);
} // Debugger

namespace Kernel
{
    void dbg_format(const char16*, ...);

#if ! defined(_DEBUG)
    #define DBG_FORMAT  dbg_format
#else // ! defined(_DEBUG)
    #define DBG_FORMAT  __noop
#endif // ! defined(_DEBUG)
} // Kernel

#endif //!defined(INCLUDE_kernel_debugger_h)
