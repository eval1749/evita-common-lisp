//////////////////////////////////////////////////////////////////////////////
//
// Editor - Edit Mode - C++ Mode
// listener/winapp/mode_Cxx.h
//
// Copyright (C) 1996-2007 by Project Vogue.
// Written by Yoshifumi "VOGUE" INOUE. (yosi@msn.com)
//
// @(#)$Id: //proj/evcl3/mainline/listener/winapp/mode_Cxx.h#7 $
//
#if !defined(INCLUDE_mode_Cxx_h)
#define INCLUDE_mode_Cxx_h

#include "./ed_Mode.h"

#include "./ed_Buffer.h"

namespace Edit
{

/// <summary>
///  C++ mode factory
/// </summary>
class CxxModeFactory : public ModeFactory
{
    // ctor
    public: CxxModeFactory();

    // [C]
    public: virtual Mode* Create(Buffer*) override;

    // [G]
    protected: virtual const char16* getExtensions() const override
        { return L"cc cpp cxx c hpp hxx h css cs ev mm"; }

    public: virtual const char16* GetName() const override
        { return L"C++"; }
}; // CxxModeFactory

/// <summary>
///  Java mode factory
/// </summary>
class JavaModeFactory : public ModeFactory
{
    // ctor
    public: JavaModeFactory();

    // [C]
    public: virtual Mode* Create(Buffer*) override;

    // [G]
    protected: virtual const char16* getExtensions() const override
        { return L"java js"; }

    public: virtual const char16* GetName() const override
        { return L"Java"; }
}; // JavaModeFactory

} // Edit

#endif //!defined(INCLUDE_mode_Cxx_h)
