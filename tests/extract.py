#!/usr/bin/env python3
"""Pull the pure decoder functions out of the MPW sources (CR line endings,
MacRoman) into harness_funcs.inc so decoder-tests.cpp can test them on a
modern machine with the Newton runtime stubbed out."""
import os
here = os.path.dirname(os.path.abspath(__file__))
sm = os.path.join(here, "..", "source", "NCT_Projects", "SM_NCT", "SM")
def load(name):
    return open(os.path.join(sm, name), encoding="latin-1").read().replace("\r", "\n")
def func(s, sig):
    i = s.index(sig); j = s.index("{", i); depth = 0; k = j
    while True:
        if s[k] == "{": depth += 1
        elif s[k] == "}":
            depth -= 1
            if depth == 0: return s[i:k+1]
        k += 1
src, um = load("DecodeFunc.cp"), load("UnicodeMapper.cp")
parts = [func(src, "static int IsHexChar"), func(src, "unsigned char ToHex(UniChar ch)"),
         func(src, "Ref DecodeQuotedPrintable(RefArg rcvr, RefArg ioText)"),
         func(src, "Ref DecodeBase64(RefArg rcvr, RefArg ioText)"),
         func(um, "static long	ConvertToUnicodeFunc_UTF8")]
open(os.path.join(here, "harness_funcs.inc"), "w", encoding="latin-1").write("\n\n".join(parts))
print("wrote harness_funcs.inc")
