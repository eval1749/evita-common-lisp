//////////////////////////////////////////////////////////////////////////////
//
// evcl - genesis - ir - Types
// ir_type.h
//
// Copyright (C) 1996-2006 by Project Vogue.
// Written by Yoshifumi "VOGUE" INOUE. (yosi@msn.com)
//
// @(#)$Id: //proj/evcl3/mainline/compiler/ir/ir_type.h#2 $
//
#if !defined(INCLUDE_compiler_ir_type_h)
#define INCLUDE_compiler_ir_type_h

#include "../ty/ty_defs.h"

namespace Compiler
{

struct Arity;
void ir_get_ty_arity(Ty, Arity*);

} // Compiler

#endif //!defined(INCLUDE_compiler_ir_type_h)
