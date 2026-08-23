#if !defined(PX_TEXTMATE_H)
#define PX_TEXTMATE_H
/*
 * TextMate grammar generator for a parser (IDE syntax highlighting)
 *
 * Copyright 2026 Clifford Heath. ALL RIGHTS RESERVED SUBJECT TO ATTACHED LICENSE.
 */
#include	<strval.h>
#include	<variant.h>

void emit_textmate(const char* parser_name, VariantArray rules);

#endif // PX_TEXTMATE_H
