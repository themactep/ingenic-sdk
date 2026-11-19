#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""
check_sensor_modes.py - cross-check each sensor driver's modes against itself.

Run from the root of ingenic-sdk (or pass --root). Read-only: no driver is
ever modified.

A sensor driver states the same facts in several places, and the places are
edited by copy and paste:

  T  the frame length its register init table programs (the VTS registers)
  A  the frame length the mode reports to the ISP (attr total_height, set in
     the probe / attr_check / setting_select code)
  C  the frame length its own set_fps() arithmetic gives at the mode's nominal
     frame rate (sclk / hts / fps)

The ISP derives its line time from the frame rate and A, so when A is not the
frame the sensor really runs, every exposure time and every anti-flicker step
is scaled by A / real (thingino/ingenic-sdk pull request 13, gc2053 on T31).

How it works: the script does not know any sensor's register map. It parses
the driver's C source and *interprets* the driver's own code, once per mode
(every combination of sensor_max_fps / data_interface / default_boot / ...
the driver switches on):

  1. probe, attr_check, setting_select and init are executed to find the
     window setting (wsize), the init table and the attr values of the mode.
  2. set_fps() is executed with the mode's nominal fps, with register reads
     answered from the init table. That yields C, and shows which registers
     set_fps() writes the frame length to, and in what byte layout.
  3. The same registers are looked up in the init table and converted back
     through set_fps()'s own arithmetic. That yields T.

Checks (see --list-checks): frame-length, byte-order, frame-short, fps-clock,
fps-range, integration-limit, image-size, dup-enum, and the low-severity
max-fps, table-name, line-time, total-width. Findings of medium severity and
above are listed by default; the summary table counts all of them.

A finding says that two places in one driver disagree. It does not say which
of them is right: that takes the sensor's datasheet or a measurement.

A run can be compared with a saved one: --write-baseline FILE records the
findings, --baseline FILE lists only those that are not in it. With
--fail-on, a driver that cannot be parsed fails the run like a finding.

Needs Python 3.7 or later (it relies on dicts keeping insertion order);
tested with 3.11, 3.12 and 3.14. Standard library only. The tests are in
tools/tests/test_check_sensor_modes.py.
"""

import argparse
import collections
import fnmatch
import hashlib
import json
import os
import re
import subprocess
import sys

VERSION = "2"       # of the output format (--json, --write-baseline)

SEVERITIES = ("high", "medium", "low")

CHECKS = {
    "frame-length": (
        "exact",
        "the init table's frame-length registers (T) disagree with the "
        "total_height the mode reports (A). High when set_fps() arithmetic "
        "(C) sides with the table or cannot be read: the ISP's line time is "
        "then wrong (the pull request 13 pattern). Medium when C sides with "
        "A: the line time is right but the sensor starts at another frame "
        "rate than the mode states."),
    "fps-clock": (
        "exact arithmetic, but trusts the driver's sclk constant",
        "sclk / hts / nominal fps (C) disagrees with total_height (A) while "
        "the table agrees with A or does not program the registers: the "
        "sclk constant, the hts, or the mode's fps label is wrong."),
    "frame-short": (
        "exact",
        "total_height is smaller than the image height of the mode's window."),
    "integration-limit": (
        "heuristic (exposure units differ between sensors)",
        "max_integration_time, max_integration_time_native or "
        "integration_time_limit is longer than total_height, and is not the "
        "2 x total_height of a sensor whose exposure counts half lines."),
    "image-size": (
        "heuristic (a driver may crop on purpose)",
        "the window's width or height disagrees with the mipi/dvp bus "
        "image_twidth / image_theight copied into the attr for that mode."),
    "fps-range": (
        "exact",
        "set_fps() rejects the mode's own nominal frame rate with its range "
        "check."),
    "max-fps": (
        "heuristic (assumes frame length >= image height)",
        "at the highest rate set_fps() accepts for the mode, the frame it "
        "computes is shorter than the image height."),
    "dup-enum": (
        "exact",
        "two constants a driver uses to tell modes apart have the same value "
        "in this SoC's tx-isp-common.h, so one of the modes cannot be "
        "selected."),
    "byte-order": (
        "exact",
        "set_fps() writes the two frame-length bytes to the registers in the "
        "opposite order to the init table: a set_fps() call misprograms the "
        "frame."),
    "table-name": (
        "heuristic (names and labels only)",
        "the init table's name states another size or frame rate than the "
        "window setting that uses it."),
    "line-time": (
        "heuristic (use of the field by the ISP not known)",
        "one_line_expr_in_us disagrees with hts / sclk."),
    "total-width": (
        "heuristic (units differ between drivers)",
        "total_width disagrees with the hts set_fps() reads or uses."),
}

# --------------------------------------------------------------------------
# source text: comments, preprocessor, tokens
# --------------------------------------------------------------------------


def strip_comments(src):
    out = []
    i = 0
    n = len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(re.sub(r"[^\n]", " ", src[i:j]))
            i = j
        elif c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == '"' or c == "'":
            j = i + 1
            while j < n and src[j] != c and src[j] != "\n":
                if src[j] == "\\":
                    j += 1
                j += 1
            out.append(src[i:j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def pp_condition(kw, rest, defines, unread=None, line=0):
    rest = rest.strip()
    if kw == "ifdef":
        return rest.split()[0] in defines if rest else False
    if kw == "ifndef":
        return rest.split()[0] not in defines if rest else True
    try:
        return bool(pp_eval(rest, defines))
    except ValueError as e:
        # not an integer constant expression the checker reads: the block is
        # left out, and the driver's notes say so
        if unread is not None:
            unread.append((line, rest, str(e)))
        return False


_PP_TOKEN = re.compile(r"\s*(?:(0[xX][0-9a-fA-F]+|\d+)[uUlL]*|([A-Za-z_]\w*)"
                       r"|(\|\||&&|==|!=|<=|>=|<<|>>|[-+*/%&|^~!<>()?:]))")

_PP_BINARY = (("||",), ("&&",), ("|",), ("^",), ("&",), ("==", "!="),
              ("<", "<=", ">", ">="), ("<<", ">>"), ("+", "-"), ("*", "/", "%"))


def pp_eval(text, defines, depth=0):
    """Value of a #if expression: integer constants, defined(), object-like
    macros of this file, and C's integer operators. Raises ValueError for
    anything else (a function-like macro, a division by zero, a shift by
    64 or more), so that nothing is ever handed to eval()."""
    if depth > 16:
        raise ValueError("macro nesting")
    toks = []
    pos = 0
    text = text.strip()
    while pos < len(text):
        m = _PP_TOKEN.match(text, pos)
        if not m:
            raise ValueError("unexpected %r" % text[pos:pos + 8])
        toks.append((m.lastindex, m.group(m.lastindex)))
        pos = m.end()
    state = [0]

    def peek():
        return toks[state[0]][1] if state[0] < len(toks) else None

    def take():
        if state[0] >= len(toks):
            raise ValueError("expression ends early")
        t = toks[state[0]]
        state[0] += 1
        return t

    def primary():
        kind, t = take()
        if kind == 1:
            return int(t, 0)
        if kind == 2:
            if t == "defined":
                paren = peek() == "("
                if paren:
                    take()
                kind, name = take()
                if kind != 2:
                    raise ValueError("defined without a name")
                if paren and take()[1] != ")":
                    raise ValueError("defined( without )")
                return 1 if name in defines else 0
            if peek() == "(":
                raise ValueError("function-like macro %s" % t)
            body = defines.get(t)
            if body is None:
                return 0        # C: an identifier that is not a macro is 0
            if not body[0].strip():
                raise ValueError("macro %s has no value" % t)
            return pp_eval(body[0], defines, depth + 1)
        if t == "(":
            v = ternary()
            if take()[1] != ")":
                raise ValueError("missing )")
            return v
        raise ValueError("unexpected %s" % t)

    def unary():
        t = peek()
        if t in ("!", "~", "-", "+"):
            take()
            v = unary()
            return int(not v) if t == "!" else ~v if t == "~" else -v if t == "-" else v
        return primary()

    def binary(level):
        if level == len(_PP_BINARY):
            return unary()
        a = binary(level + 1)
        while peek() in _PP_BINARY[level]:
            op = take()[1]
            b = binary(level + 1)
            if op == "||":
                a = int(bool(a) or bool(b))
            elif op == "&&":
                a = int(bool(a) and bool(b))
            elif op == "|":
                a |= b
            elif op == "^":
                a ^= b
            elif op == "&":
                a &= b
            elif op == "==":
                a = int(a == b)
            elif op == "!=":
                a = int(a != b)
            elif op == "<":
                a = int(a < b)
            elif op == "<=":
                a = int(a <= b)
            elif op == ">":
                a = int(a > b)
            elif op == ">=":
                a = int(a >= b)
            elif op in ("<<", ">>"):
                if not 0 <= b < 64:
                    raise ValueError("shift by %d" % b)
                a = (a << b) if op == "<<" else (a >> b)
            elif op == "+":
                a += b
            elif op == "-":
                a -= b
            elif op == "*":
                a *= b
            else:
                if b == 0:
                    raise ValueError("division by zero")
                q = abs(a) // abs(b)            # C truncates toward zero
                q = q if (a < 0) == (b < 0) else -q
                a = q if op == "/" else a - q * b
            if not -(1 << 64) < a < (1 << 64):
                raise ValueError("value out of range")
        return a

    def ternary():
        c = binary(0)
        if peek() == "?":
            take()
            a = ternary()
            if take()[1] != ":":
                raise ValueError("? without :")
            b = ternary()
            return a if c else b
        return c

    v = ternary()
    if state[0] != len(toks):
        raise ValueError("unexpected %s" % toks[state[0]][1])
    return v


def preprocess(text, predefined, unread=None):
    """Resolve #if blocks (file-local macros only), collect object-like
    #defines. Dropped lines are blanked so line numbers stay true."""
    lines = text.split("\n")
    defines = dict(predefined)
    out = []
    stack = []
    i = 0
    while i < len(lines):
        line = lines[i]
        s = line.strip()
        if s.startswith("#"):
            full = s
            k = i
            while full.endswith("\\") and k + 1 < len(lines):
                k += 1
                full = full[:-1] + " " + lines[k].strip()
            d = full[1:].strip()
            m = re.match(r"(\w+)\s*(.*)", d)
            kw = m.group(1) if m else ""
            rest = m.group(2) if m else ""
            active = all(f[2] for f in stack)
            if kw in ("if", "ifdef", "ifndef"):
                cond = pp_condition(kw, rest, defines, unread, i + 1) if active else False
                stack.append([active, cond, active and cond])
            elif kw == "elif" and stack:
                f = stack[-1]
                if f[1]:
                    f[2] = False
                else:
                    cond = pp_condition("if", rest, defines, unread, i + 1) if f[0] else False
                    f[1] = cond
                    f[2] = f[0] and cond
            elif kw == "else" and stack:
                f = stack[-1]
                f[2] = f[0] and not f[1]
                f[1] = True
            elif kw == "endif" and stack:
                stack.pop()
            elif kw == "define" and active:
                m2 = re.match(r"(\w+)(\([^)]*\))?\s*(.*)", rest)
                if m2 and not m2.group(2):
                    defines[m2.group(1)] = (m2.group(3), i + 1)
            elif kw == "undef" and active:
                defines.pop(rest.strip(), None)
            for _ in range(i, k + 1):
                out.append("")
            i = k + 1
            continue
        out.append(line if all(f[2] for f in stack) else "")
        i += 1
    return "\n".join(out), defines


TOKEN_RE = re.compile(r"""
  (?P<ws>\s+)
 |(?P<num>0[xX][0-9a-fA-F]+[uUlL]*|\d+\.\d*(?:[eE][-+]?\d+)?[fF]?|\d+[uUlL]*)
 |(?P<id>[A-Za-z_]\w*)
 |(?P<str>"(?:\\.|[^"\\\n])*")
 |(?P<chr>'(?:\\.|[^'\\\n])*')
 |(?P<op><<=|>>=|\.\.\.|->|\+\+|--|<<|>>|<=|>=|==|!=|&&|\|\||\+=|-=|\*=|/=|%=|&=|\|=|\^=
        |[-+*/%&|^~!<>=?:;,.(){}\[\]])
 |(?P<bad>.)
""", re.X)


class Tok(object):
    __slots__ = ("kind", "text", "line")

    def __init__(self, kind, text, line):
        self.kind = kind
        self.text = text
        self.line = line

    def __repr__(self):
        return "%s@%d" % (self.text, self.line)


def tokenize(text):
    toks = []
    line = 1
    for m in TOKEN_RE.finditer(text):
        kind = m.lastgroup
        s = m.group(0)
        if kind == "ws":
            line += s.count("\n")
            continue
        if kind == "bad":
            continue
        toks.append(Tok(kind, s, line))
    return toks


class ParseError(Exception):
    pass


def match_close(toks, i):
    """Index of the bracket closing toks[i]."""
    pairs = {"(": ")", "[": "]", "{": "}"}
    depth = 0
    n = len(toks)
    j = i
    while j < n:
        t = toks[j].text
        if t in pairs:
            depth += 1
        elif t in (")", "]", "}"):
            depth -= 1
            if depth == 0:
                return j
        j += 1
    raise ParseError("unbalanced %r at line %d" % (toks[i].text, toks[i].line))


def match_open(toks, i):
    depth = 0
    j = i
    while j >= 0:
        t = toks[j].text
        if t in (")", "]", "}"):
            depth += 1
        elif t in ("(", "[", "{"):
            depth -= 1
            if depth == 0:
                return j
        j -= 1
    raise ParseError("unbalanced %r at line %d" % (toks[i].text, toks[i].line))


def bracket_mismatch(toks):
    """Where the brackets of a file stop pairing up, as text, or None."""
    pairs = {")": "(", "]": "[", "}": "{"}
    stack = []
    for t in toks:
        if t.text in ("(", "[", "{"):
            stack.append(t)
        elif t.text in pairs:
            if not stack or stack[-1].text != pairs[t.text]:
                if stack:
                    return "%r at line %d closes %r opened at line %d" % (
                        t.text, t.line, stack[-1].text, stack[-1].line)
                return "%r at line %d closes nothing" % (t.text, t.line)
            stack.pop()
    if stack:
        return "%r opened at line %d is never closed" % (stack[-1].text, stack[-1].line)
    return None


def split_top(toks, sep=","):
    parts = []
    cur = []
    depth = 0
    for t in toks:
        if t.text in ("(", "[", "{"):
            depth += 1
        elif t.text in (")", "]", "}"):
            depth -= 1
        if depth == 0 and t.text == sep:
            parts.append(cur)
            cur = []
        else:
            cur.append(t)
    if cur:
        parts.append(cur)
    return parts


# --------------------------------------------------------------------------
# expression and statement parser (the subset these drivers use)
# --------------------------------------------------------------------------

TYPEWORDS = set("""unsigned signed char short int long void float double const
volatile struct enum union static register inline extern
u8 u16 u32 u64 s8 s16 s32 s64 __u8 __u16 __u32 __u64 uint8_t uint16_t uint32_t
uint64_t int8_t int16_t int32_t int64_t size_t ssize_t bool uint ulong ushort
uchar unchar u_int8_t u_int16_t u_int32_t""".split())

W8 = set("u8 __u8 uint8_t uchar unchar u_int8_t".split())
W16 = set("u16 __u16 uint16_t ushort u_int16_t".split())
W32 = set("u32 __u32 uint32_t uint ulong u_int32_t size_t".split())

BINPREC = {"||": 1, "&&": 2, "|": 3, "^": 4, "&": 5, "==": 6, "!=": 6,
           "<": 7, ">": 7, "<=": 7, ">=": 7, "<<": 8, ">>": 8,
           "+": 9, "-": 9, "*": 10, "/": 10, "%": 10}
ASSIGN_OPS = set(["=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="])


def type_width(words, pointer):
    """Bit width to truncate to for an unsigned integer type, else None."""
    if pointer:
        return None
    ws = set(words)
    if ws & W8:
        return 8
    if ws & W16:
        return 16
    if ws & W32:
        return 32
    if "unsigned" in ws:
        if "char" in ws:
            return 8
        if "short" in ws:
            return 16
        if "long" in ws and words.count("long") > 1:
            return None
        return 32
    return None


class Parser(object):
    def __init__(self, toks):
        self.toks = toks
        self.pos = 0
        self.n = len(toks)

    def peek(self, off=0):
        p = self.pos + off
        return self.toks[p] if p < self.n else None

    def text(self, off=0):
        t = self.peek(off)
        return t.text if t is not None else None

    def next(self):
        t = self.peek()
        if t is None:
            raise ParseError("unexpected end of input")
        self.pos += 1
        return t

    def expect(self, text):
        t = self.next()
        if t.text != text:
            raise ParseError("expected %r, got %r at line %d" % (text, t.text, t.line))
        return t

    # ---- expressions ----

    def parse_expr(self):
        e = self.parse_assign()
        while self.text() == ",":
            self.next()
            e = ("comma", e, self.parse_assign())
        return e

    def parse_assign(self):
        lhs = self.parse_tern()
        if self.text() in ASSIGN_OPS:
            op = self.next()
            rhs = self.parse_assign()
            return ("assign", op.text, lhs, rhs, op.line)
        return lhs

    def parse_tern(self):
        c = self.parse_bin(1)
        if self.text() == "?":
            self.next()
            a = self.parse_expr()
            self.expect(":")
            b = self.parse_tern()
            return ("tern", c, a, b)
        return c

    def parse_bin(self, minp):
        lhs = self.parse_unary()
        while True:
            op = self.text()
            p = BINPREC.get(op)
            if p is None or p < minp:
                return lhs
            self.next()
            rhs = self.parse_bin(p + 1)
            lhs = ("bin", op, lhs, rhs)

    def is_cast(self):
        if self.text() != "(":
            return None
        j = self.pos + 1
        words = []
        pointer = False
        after_tag = False
        while j < self.n and self.toks[j].text != ")":
            t = self.toks[j]
            if t.text == "*":
                pointer = True
            elif t.kind == "id" and (t.text in TYPEWORDS or after_tag):
                words.append(t.text)
                after_tag = t.text in ("struct", "enum", "union")
            else:
                return None
            j += 1
        if j >= self.n or not words:
            return None
        return (j, words, pointer)

    def parse_unary(self):
        t = self.peek()
        if t is None:
            raise ParseError("unexpected end of expression")
        if t.text in ("!", "~", "-", "+", "&", "*"):
            self.next()
            return ("un", t.text, self.parse_unary())
        if t.text in ("++", "--"):
            self.next()
            e = self.parse_unary()
            return ("assign", "+=" if t.text == "++" else "-=", e, ("num", 1), t.line)
        if t.text == "sizeof":
            self.next()
            if self.text() == "(":
                self.pos = match_close(self.toks, self.pos) + 1
            else:
                self.parse_unary()
            return ("unk",)
        cast = self.is_cast()
        if cast is not None:
            j, words, pointer = cast
            self.pos = j + 1
            if self.text() == "{":
                raise ParseError("compound literal at line %d" % t.line)
            return ("cast", type_width(words, pointer), self.parse_unary())
        return self.parse_postfix()

    def parse_postfix(self):
        e = self.parse_primary()
        while True:
            t = self.peek()
            if t is None:
                return e
            if t.text == "(":
                self.next()
                args = []
                if self.text() != ")":
                    args.append(self.parse_arg())
                    while self.text() == ",":
                        self.next()
                        args.append(self.parse_arg())
                self.expect(")")
                e = ("call", e, args, t.line)
            elif t.text == "[":
                self.next()
                i = self.parse_expr()
                self.expect("]")
                e = ("idx", e, i)
            elif t.text == ".":
                self.next()
                e = ("mem", e, self.next().text, False)
            elif t.text == "->":
                self.next()
                e = ("mem", e, self.next().text, True)
            elif t.text in ("++", "--"):
                self.next()
                e = ("assign", "+=" if t.text == "++" else "-=", e, ("num", 1), t.line)
            else:
                return e

    def parse_arg(self):
        """A call argument; a type name (container_of, sizeof-like macros)
        is skipped as unknown."""
        t = self.peek()
        if t is not None and t.kind == "id" and t.text in TYPEWORDS:
            depth = 0
            while self.peek() is not None:
                x = self.text()
                if x in ("(", "[", "{"):
                    depth += 1
                elif x in (")", "]", "}"):
                    if depth == 0:
                        break
                    depth -= 1
                elif x == "," and depth == 0:
                    break
                self.next()
            return ("unk",)
        return self.parse_assign()

    def parse_primary(self):
        t = self.next()
        if t.kind == "num":
            s = t.text
            if s[:2] in ("0x", "0X"):
                s = s.rstrip("uUlL")
            else:
                s = s.rstrip("uUlLfF")
            try:
                return ("num", int(s, 0) if not re.match(r"0\d", s) else int(s, 8))
            except ValueError:
                try:
                    return ("num", float(s))
                except ValueError:
                    raise ParseError("bad number %r at line %d" % (t.text, t.line))
        if t.kind == "id":
            return ("id", t.text, t.line)
        if t.kind == "str":
            while self.peek() is not None and self.peek().kind == "str":
                self.next()
            return ("str", t.text)
        if t.kind == "chr":
            body = t.text[1:-1]
            return ("num", ord(body[-1]) if body else 0)
        if t.text == "(":
            if self.text() == "{":
                raise ParseError("statement expression at line %d" % t.line)
            e = self.parse_expr()
            self.expect(")")
            return e
        raise ParseError("unexpected %r at line %d" % (t.text, t.line))

    # ---- statements ----

    def parse_block(self):
        self.expect("{")
        stmts = []
        while self.text() != "}":
            if self.peek() is None:
                raise ParseError("unterminated block")
            stmts.append(self.parse_stmt())
        self.expect("}")
        return ("block", stmts)

    def is_decl_start(self):
        t = self.peek()
        if t.kind != "id":
            return False
        if t.text in TYPEWORDS:
            return True
        t1 = self.peek(1)
        if t1 is None:
            return False
        if t1.kind == "id" and t1.text not in ("sizeof",):
            return True
        if t1.text == "*":
            t2, t3 = self.peek(2), self.peek(3)
            if t2 is not None and t2.kind == "id" and t3 is not None and t3.text in ("=", ";", ","):
                return True
        return False

    def parse_decl(self):
        line = self.peek().line
        words = []
        after_tag = False
        while True:
            t = self.peek()
            if t is None:
                raise ParseError("unterminated declaration at line %d" % line)
            if t.kind == "id" and (t.text in TYPEWORDS or after_tag):
                words.append(t.text)
                after_tag = t.text in ("struct", "enum", "union")
                self.next()
            else:
                break
        if not words:
            words.append(self.next().text)
        decls = []
        while True:
            pointer = False
            while self.text() == "*":
                self.next()
                pointer = True
            while self.peek() is not None and self.peek().kind == "id" and self.text() in ("const", "volatile"):
                self.next()
            t = self.next()
            if t.kind != "id":
                raise ParseError("declarator expected, got %r at line %d" % (t.text, t.line))
            name = t.text
            array = False
            while self.text() == "[":
                self.pos = match_close(self.toks, self.pos) + 1
                array = True
            init = None
            aggregate = False
            if self.text() == "=":
                self.next()
                if self.text() == "{":
                    self.pos = match_close(self.toks, self.pos) + 1
                    aggregate = True
                else:
                    init = self.parse_assign()
            decls.append((name, None if array else type_width(words, pointer), init,
                          aggregate or array, t.line))
            if self.text() == ",":
                self.next()
                continue
            self.expect(";")
            break
        return ("decl", decls)

    def parse_stmt(self):
        t = self.peek()
        if t is None:
            raise ParseError("unexpected end of function")
        x = t.text
        if x == "{":
            return self.parse_block()
        if x == ";":
            self.next()
            return ("empty",)
        if t.kind == "id":
            if x == "if":
                self.next()
                self.expect("(")
                c = self.parse_expr()
                self.expect(")")
                a = self.parse_stmt()
                b = None
                if self.text() == "else":
                    self.next()
                    b = self.parse_stmt()
                return ("if", c, a, b, t.line)
            if x == "switch":
                self.next()
                self.expect("(")
                e = self.parse_expr()
                self.expect(")")
                return ("switch", e, self.parse_stmt(), t.line)
            if x == "case":
                self.next()
                e = self.parse_bin(1)
                hi = None
                if self.text() == "...":
                    self.next()
                    hi = self.parse_bin(1)
                self.expect(":")
                return ("case", e, hi)
            if x == "default" and self.text(1) == ":":
                self.next()
                self.next()
                return ("default",)
            if x == "for":
                self.next()
                self.expect("(")
                init = None
                if self.text() == ";":
                    self.next()
                elif self.is_decl_start():
                    init = self.parse_decl()
                else:
                    init = ("expr", self.parse_expr(), t.line)
                    self.expect(";")
                cond = None if self.text() == ";" else self.parse_expr()
                self.expect(";")
                step = None if self.text() == ")" else self.parse_expr()
                self.expect(")")
                return ("for", init, cond, step, self.parse_stmt())
            if x == "while":
                self.next()
                self.expect("(")
                c = self.parse_expr()
                self.expect(")")
                return ("while", c, self.parse_stmt())
            if x == "do":
                self.next()
                body = self.parse_stmt()
                self.expect("while")
                self.expect("(")
                c = self.parse_expr()
                self.expect(")")
                self.expect(";")
                return ("dowhile", body, c)
            if x == "return":
                self.next()
                e = None if self.text() == ";" else self.parse_expr()
                self.expect(";")
                return ("return", e, t.line)
            if x in ("break", "continue"):
                self.next()
                self.expect(";")
                return (x,)
            if x == "goto":
                self.next()
                name = self.next().text
                self.expect(";")
                return ("goto", name, t.line)
            if self.text(1) == ":":
                self.next()
                self.next()
                return ("label", x)
            if self.is_decl_start():
                return self.parse_decl()
        e = self.parse_expr()
        self.expect(";")
        return ("expr", e, t.line)


# --------------------------------------------------------------------------
# translation unit
# --------------------------------------------------------------------------

class Glob(object):
    def __init__(self, name, type_text, is_array, init, line):
        self.name = name
        self.type_text = type_text
        self.is_array = is_array
        self.init = init          # token list or None
        self.line = line
        self._agg = None

    def agg(self):
        if self._agg is None and self.init and self.init[0].text == "{":
            self._agg = parse_aggregate(self.init)
        return self._agg


def parse_aggregate(toks):
    """{ .a = x, .b = { ... }, y } -> ('agg', [(designator or None, value)])
    where value is ('agg', ...) or ('expr', tokens)."""
    if not toks or toks[0].text != "{":
        return ("expr", toks)
    end = match_close(toks, 0)
    items = []
    for part in split_top(toks[1:end]):
        if not part:
            continue
        desig = None
        if part[0].text == "." or part[0].text == "[":
            eq = None
            depth = 0
            for k, t in enumerate(part):
                if t.text in ("(", "[", "{"):
                    depth += 1
                elif t.text in (")", "]", "}"):
                    depth -= 1
                elif t.text == "=" and depth == 0:
                    eq = k
                    break
            if eq is None:
                continue
            desig = "".join(t.text for t in part[:eq]).lstrip(".")
            part = part[eq + 1:]
            if not part:
                continue
        items.append((desig, parse_aggregate(part), part[0].line))
    return ("agg", items)


def flatten_struct(agg, prefix=""):
    """Designated struct initializer -> {dotted field: (tokens, line)}."""
    out = {}
    if agg is None or agg[0] != "agg":
        return out
    for desig, value, line in agg[1]:
        if desig is None:
            continue
        key = prefix + desig
        if value[0] == "agg":
            out.update(flatten_struct(value, key + "."))
        else:
            out[key] = (value[1], line)
    return out


class Func(object):
    def __init__(self, name, params, body, line):
        self.name = name
        self.params = params      # list of parameter names
        self.body_toks = body
        self.line = line
        self.ast = None
        self.error = None

    def get_ast(self):
        if self.ast is None and self.error is None:
            try:
                p = Parser(self.body_toks)
                self.ast = p.parse_block()
            except ParseError as e:
                self.error = str(e)
            except RecursionError:
                self.error = "expression too deep"
        if self.error is not None:
            raise ParseError("%s(): %s" % (self.name, self.error))
        return self.ast


class Unit(object):
    def __init__(self, path, rel, text, predefined):
        self.path = path
        self.rel = rel
        self.unread_conditions = []
        clean, self.defines = preprocess(strip_comments(text), predefined, self.unread_conditions)
        self.toks = tokenize(clean)
        self.funcs = {}
        self.globs = {}
        self.module_params = set()
        try:
            self._scan()
        except ParseError as e:
            bad = bracket_mismatch(self.toks)
            if bad is None:
                raise
            raise ParseError("the source's brackets do not pair up: %s (does this file compile?)" % bad)
        self._flat = {}
        self._tables = {}

    def _scan(self):
        toks = self.toks
        n = len(toks)
        i = 0
        start = 0
        while i < n:
            t = toks[i].text
            if t == "{":
                j = match_close(toks, i)
                if i > 0 and toks[i - 1].text == ")":
                    p = match_open(toks, i - 1)
                    if p > 0 and toks[p - 1].kind == "id":
                        name = toks[p - 1].text
                        params = []
                        for part in split_top(toks[p + 1:i - 1]):
                            ids = [x.text for x in part if x.kind == "id"]
                            if ids:
                                params.append(ids[-1])
                        if name not in self.funcs:
                            self.funcs[name] = Func(name, params, toks[i:j + 1], toks[p - 1].line)
                    i = j + 1
                    start = i
                    continue
                i = j + 1
                continue
            if t == "(" or t == "[":
                i = match_close(toks, i) + 1
                continue
            if t == ";":
                self._decl(toks[start:i])
                i += 1
                start = i
                continue
            i += 1

    def _decl(self, toks):
        if not toks:
            return
        if toks[0].text == "module_param" and len(toks) > 2:
            self.module_params.add(toks[2].text)
            return
        if toks[0].text == "typedef":
            return
        eq = None
        depth = 0
        for k, t in enumerate(toks):
            if t.text in ("(", "[", "{"):
                depth += 1
            elif t.text in (")", "]", "}"):
                depth -= 1
            elif t.text == "=" and depth == 0:
                eq = k
                break
        lhs = toks if eq is None else toks[:eq]
        init = None if eq is None else toks[eq + 1:]
        if any(t.text == "(" for t in lhs) or any(t.text == "{" for t in lhs):
            return
        name = None
        is_array = False
        for k, t in enumerate(lhs):
            if t.text == "[":
                is_array = True
                break
            if t.kind == "id":
                name = t
        if name is None or name.text in TYPEWORDS:
            return
        idx = lhs.index(name)
        type_text = " ".join(t.text for t in lhs[:idx])
        if not type_text:
            return
        old = self.globs.get(name.text)
        if old is not None and old.init is not None and init is None:
            return
        self.globs[name.text] = Glob(name.text, type_text, is_array, init, name.line)

    # struct globals

    def flat(self, name, index=None):
        """Flattened designated fields of a struct global (or of element
        `index` of an array of structs). None if not such a global."""
        key = (name, index)
        if key in self._flat:
            return self._flat[key]
        g = self.globs.get(name)
        res = None
        if g is not None:
            agg = g.agg()
            if agg is not None and agg[0] == "agg":
                if index is None and not g.is_array:
                    res = flatten_struct(agg)
                elif index is not None and g.is_array:
                    items = [it for it in agg[1]]
                    if 0 <= index < len(items) and items[index][1][0] == "agg":
                        res = flatten_struct(items[index][1])
            elif g.init is None and index is None and not g.is_array and "struct" in g.type_text:
                res = {}
        self._flat[key] = res
        return res

    def array_len(self, name):
        g = self.globs.get(name)
        if g is None or not g.is_array:
            return None
        agg = g.agg()
        if agg is None or agg[0] != "agg":
            return None
        return len(agg[1])

    def find_glob(self, type_word, want_array):
        best = None
        for g in self.globs.values():
            if type_word in g.type_text.split() and g.is_array == want_array and "*" not in g.type_text:
                if best is None or (g.init is not None and best.init is None):
                    best = g
        return best


# --------------------------------------------------------------------------
# values and interpreter
# --------------------------------------------------------------------------

class _Unknown(object):
    def __repr__(self):
        return "?"


UNK = _Unknown()


class Sym(object):
    __slots__ = ("name",)

    def __init__(self, name):
        self.name = name

    def __eq__(self, other):
        return isinstance(other, Sym) and other.name == self.name

    def __ne__(self, other):
        return not self.__eq__(other)

    def __hash__(self):
        return hash(("sym", self.name))

    def __repr__(self):
        return self.name


class Ptr(object):
    __slots__ = ("loc",)

    def __init__(self, loc):
        self.loc = loc

    def __repr__(self):
        return "&%s" % (self.loc[1],)


class ReturnEx(Exception):
    def __init__(self, value):
        self.value = value


class BreakEx(Exception):
    pass


class ContinueEx(Exception):
    pass


class GotoEx(Exception):
    def __init__(self, label, line):
        self.label = label
        self.line = line


class Abort(Exception):
    pass


LOG_FUNCS = set("ISP_ERROR ISP_WARNING ISP_INFO printk pr_err pr_info pr_debug pr_warn "
                "dev_err dev_info dev_dbg ISP_PRINT".split())
SKIP_CALLEE = re.compile(r"(_read|_write|_read_array|_write_array|_detect|_reset|_clk_set|"
                         r"_g_chip_ident|_s_stream|_probe|_remove|_ioctl|_set_fps|"
                         r"_set_integration_time|_set_analog_gain|_set_expo|_set_logic)$")
TRACKED = frozenset(["total_height", "total_width", "wsize", "max_integration_time",
                     "integration_time_limit", "max_integration_time_native"])
GUARDED = frozenset(["total_height"])


_MENTIONS = {}


def mentions(node, names, _memo=_MENTIONS):
    # keyed by id(): the cache is emptied for every driver (see analyze)
    key = (id(node), names)
    if key in _memo:
        return _memo[key]
    res = False
    if isinstance(node, tuple):
        if node and node[0] == "id" and node[1] in names:
            res = True
        elif node and node[0] == "mem" and node[2] in names:
            res = True
        else:
            for x in node:
                if isinstance(x, (tuple, list)) and mentions(x, names):
                    res = True
                    break
    elif isinstance(node, list):
        for x in node:
            if isinstance(x, (tuple, list)) and mentions(x, names):
                res = True
                break
    _memo[key] = res
    return res


def ast_text(e):
    k = e[0]
    if k == "id":
        return e[1]
    if k == "mem":
        return ast_text(e[1]) + ("->" if e[3] else ".") + e[2]
    if k == "idx":
        i = e[2]
        return ast_text(e[1]) + "[" + (str(i[1]) if i[0] == "num" else "?") + "]"
    if k == "cast":
        return ast_text(e[2])
    if k == "un" and e[1] in ("&", "*"):
        return e[1] + ast_text(e[2])
    if k == "num":
        return str(e[1])
    return "?"


INFO_RE = re.compile(r"(?:^|->|\.)info(?:->|\.)(\w+)$")
ATTR_RE = re.compile(r"(?:^|->|\.)attr->(.+)$")


class Interp(object):
    def __init__(self, unit, consts, attr_name):
        self.unit = unit
        self.consts = consts
        self.attr_name = attr_name
        self.store = {}
        self.lines = {}
        self.frames = []
        self.widths = []
        self.depth = 0
        self.steps = 0
        self.macro_busy = set()
        self.cur_win = None
        self.called = set()
        self.nomatch = []        # definite switch values that matched no case
        self.unknown_guard = []  # lines of unknown conditions guarding mode code
        self.gotos = []          # (function, label, line) of jumps that were not followed
        self.jumps = 0
        self.copies = {}
        self.dyn = set()         # global keys assigned by executed code
        # set_fps instrumentation
        self.table = None
        self.page_reg = None
        self.page_cur = None
        self.writes = []
        self.reads = []
        self.inject_var = None
        self.inject_val = None
        self.inject_param = None
        self.tainted = None      # frozenset: the fps argument and the locals computed from it
        self.injected = False
        self.skip_return = None
        self.top_locals = None

    def clone(self):
        o = Interp(self.unit, self.consts, self.attr_name)
        o.store = dict(self.store)
        o.lines = dict(self.lines)
        o.cur_win = self.cur_win
        o.copies = dict(self.copies)
        o.dyn = set(self.dyn)
        return o

    # ---- locations ----

    def canon(self, key):
        m = INFO_RE.search(key)
        if m:
            return "@" + m.group(1)
        m = ATTR_RE.search(key)
        if m and self.attr_name:
            return self.attr_name + "." + m.group(1)
        return key

    def loc(self, e):
        k = e[0]
        if k == "id":
            name = e[1]
            if self.frames and name in self.frames[-1]:
                return ("L", name)
            return ("G", name)
        if k == "mem":
            if e[3]:
                pv = self.eval(e[1])
                if isinstance(pv, Ptr):
                    base = pv.loc
                    if base[0] == "L":
                        return ("L", base[1] + "." + e[2])
                    return ("G", self.canon(base[1] + "." + e[2]))
                return ("G", self.canon(ast_text(e[1]) + "->" + e[2]))
            b = self.loc(e[1])
            if b is None:
                return None
            if b[0] == "L":
                return ("L", b[1] + "." + e[2])
            return ("G", self.canon(b[1] + "." + e[2]))
        if k == "idx":
            b = self.loc(e[1])
            i = self.eval(e[2])
            if b is None or not isinstance(i, int):
                return None
            return (b[0], "%s[%d]" % (b[1], i))
        if k == "un" and e[1] == "*":
            pv = self.eval(e[2])
            if isinstance(pv, Ptr):
                return pv.loc
            return None
        if k == "cast":
            return self.loc(e[2])
        return None

    def get(self, loc):
        if loc is None:
            return UNK
        if loc[0] == "L":
            return self.frames[-1].get(loc[1], UNK)
        key = loc[1]
        if key in self.store:
            return self.store[key]
        v = self.static_value(key)
        self.store[key] = v
        return v

    def static_value(self, key):
        m = re.match(r"([A-Za-z_]\w*)(?:\[(\d+)\])?(?:\.(.+))?$", key)
        if not m:
            return UNK
        name, idx, field = m.group(1), m.group(2), m.group(3)
        g = self.unit.globs.get(name)
        if g is None:
            return UNK
        if field is None:
            if idx is None and g.is_array:
                return Ptr(("G", name))     # an array name decays to a pointer
            if idx is not None:
                return UNK
            if g.init is None:
                return 0 if "struct" not in g.type_text and "*" not in g.type_text else UNK
            if g.init[0].text == "{":
                return UNK
            return self.eval_tokens(g.init)
        flat = self.unit.flat(name, int(idx) if idx is not None else None)
        if flat is None:
            return UNK
        ent = flat.get(field)
        if ent is None:
            for k in flat:
                if k.startswith(field + "."):
                    return UNK
            return 0
        self.lines.setdefault(key, ent[1])
        return self.eval_tokens(ent[0])

    def eval_tokens(self, toks):
        try:
            p = Parser(toks)
            e = p.parse_expr()
        except (ParseError, RecursionError):
            return UNK
        saved = self.frames
        self.frames = []
        try:
            return self.eval(e)
        finally:
            self.frames = saved

    def set(self, loc, value, line=None):
        if loc is None:
            return
        if loc[0] == "L":
            w = self.widths[-1].get(loc[1])
            if w and isinstance(value, int):
                value &= (1 << w) - 1
            self.frames[-1][loc[1]] = value
        else:
            self.store[loc[1]] = value
            self.dyn.add(loc[1])
            if line is not None:
                self.lines[loc[1]] = line
        if isinstance(value, Ptr) and value.loc[0] == "G" and re.match(r"\w+\[\d+\]$", value.loc[1]) \
                and re.match(r"\w+$", loc[1]):
            name = value.loc[1].split("[")[0]
            g = self.unit.globs.get(name)
            if g is not None and "tx_isp_sensor_win_setting" in g.type_text:
                self.cur_win = value.loc[1]

    # ---- expressions ----

    def ident(self, name):
        if self.frames and name in self.frames[-1]:
            return self.frames[-1][name]
        if name in self.store:
            return self.store[name]
        d = self.unit.defines.get(name)
        if d is not None:
            if name in self.macro_busy or not d[0].strip():
                return UNK
            self.macro_busy.add(name)
            try:
                return self.eval_tokens(tokenize(d[0]))
            finally:
                self.macro_busy.discard(name)
        if name in self.consts:
            return self.consts[name]
        if name in self.unit.globs:
            return self.get(("G", name))
        if name == "NULL":
            return 0
        m = re.match(r"TX_SENSOR_MAX_FPS_(\d+)$", name)
        if m:
            return int(m.group(1))
        if name.isupper() or re.match(r"[A-Z][A-Z0-9_]*$", name):
            return Sym(name)
        return UNK

    @staticmethod
    def truth(v):
        if isinstance(v, bool):
            return v
        if isinstance(v, (int, float)):
            return v != 0
        if isinstance(v, Ptr):
            return True
        return UNK

    def binop(self, op, a, b):
        if op in ("==", "!="):
            if isinstance(a, (int, float)) and isinstance(b, (int, float)):
                r = a == b
            elif isinstance(a, Sym) and isinstance(b, Sym):
                r = a == b
            elif isinstance(a, Ptr) and isinstance(b, Ptr):
                r = a.loc == b.loc
            elif isinstance(a, Ptr) and b == 0 or isinstance(b, Ptr) and a == 0:
                r = False
            else:
                return UNK
            return int(r if op == "==" else not r)
        if not isinstance(a, (int, float)) or not isinstance(b, (int, float)):
            return UNK
        try:
            if op == "+":
                return a + b
            if op == "-":
                return a - b
            if op == "*":
                r = a * b
                if isinstance(r, int) and r >= (1 << 32):
                    r &= 0xffffffff
                return r
            if op in ("/", "%"):
                if b == 0:
                    return UNK
                if isinstance(a, float) or isinstance(b, float):
                    return a / b if op == "/" else UNK
                q = abs(a) // abs(b)
                if (a < 0) != (b < 0):
                    q = -q
                return q if op == "/" else a - q * b
            if op == "<":
                return int(a < b)
            if op == ">":
                return int(a > b)
            if op == "<=":
                return int(a <= b)
            if op == ">=":
                return int(a >= b)
            if isinstance(a, float) or isinstance(b, float):
                return UNK
            if op == "&":
                return a & b
            if op == "|":
                return a | b
            if op == "^":
                return a ^ b
            if op == "<<":
                return (a << b) if 0 <= b < 64 else UNK
            if op == ">>":
                return (a >> b) if 0 <= b < 64 else UNK
        except (OverflowError, ValueError):
            return UNK
        return UNK

    def eval(self, e):
        k = e[0]
        if k == "num":
            return e[1]
        if k == "id":
            return self.ident(e[1])
        if k == "bin":
            op = e[1]
            if op == "&&":
                a = self.truth(self.eval(e[2]))
                if a is False:
                    return 0
                b = self.truth(self.eval(e[3]))
                if b is False:
                    return 0
                if a is UNK or b is UNK:
                    return UNK
                return 1
            if op == "||":
                a = self.truth(self.eval(e[2]))
                if a is True:
                    return 1
                b = self.truth(self.eval(e[3]))
                if b is True:
                    return 1
                if a is UNK or b is UNK:
                    return UNK
                return 0
            return self.binop(op, self.eval(e[2]), self.eval(e[3]))
        if k == "mem" or k == "idx":
            return self.get(self.loc(e))
        if k == "assign":
            return self.assign(e)
        if k == "call":
            return self.call(e)
        if k == "cast":
            v = self.eval(e[2])
            if e[1] and isinstance(v, int):
                v &= (1 << e[1]) - 1
            elif e[1] and isinstance(v, float):
                v = int(v) & ((1 << e[1]) - 1)
            return v
        if k == "un":
            op = e[1]
            if op == "&":
                l = self.loc(e[2])
                return Ptr(l) if l is not None else UNK
            if op == "*":
                pv = self.eval(e[2])
                return self.get(pv.loc) if isinstance(pv, Ptr) else UNK
            v = self.eval(e[2])
            if op == "!":
                t = self.truth(v)
                return UNK if t is UNK else int(not t)
            if not isinstance(v, (int, float)):
                return UNK
            if op == "-":
                return -v
            if op == "+":
                return v
            if op == "~":
                return ~v if isinstance(v, int) else UNK
        if k == "tern":
            c = self.truth(self.eval(e[1]))
            if c is UNK:
                return UNK
            return self.eval(e[2] if c else e[3])
        if k == "comma":
            self.eval(e[1])
            return self.eval(e[2])
        return UNK

    def assign(self, e):
        op, lhs, rhs, line = e[1], e[2], e[3], e[4]
        val = self.eval(rhs)
        l = self.loc(lhs)
        if op != "=":
            val = self.binop(op[:-1], self.get(l), val)
        if self.tainted is not None and l is not None and l[0] == "L" and len(self.frames) == 1:
            dep = mentions(rhs, self.tainted)
            if dep and l[1] not in self.tainted:
                self.tainted = self.tainted | frozenset([l[1]])
            if (dep and self.inject_val is not None and l[1] == self.inject_var and op == "="
                    and not mentions(rhs, frozenset([l[1]]))):   # not "vts = vts - 1440"
                val = self.inject_val
                self.injected = True
        self.set(l, val, line)
        return self.get(l) if l is not None and l[0] == "L" else val

    def call(self, e):
        fn, args, line = e[1], e[2], e[3]
        if fn[0] != "id":
            return UNK
        name = fn[1]
        if len(args) == 3 and name.endswith("_read"):
            reg = self.eval(args[1])
            tgt = args[2]
            while tgt[0] == "cast":
                tgt = tgt[2]
            l = self.loc(tgt[2]) if tgt[0] == "un" and tgt[1] == "&" else None
            v = self.table_read(reg)
            self.reads.append((reg, v, line))
            self.set(l, v, line)
            return 0
        if len(args) == 3 and name.endswith("_write"):
            reg = self.eval(args[1])
            val = self.eval(args[2])
            if self.page_reg is not None and reg == self.page_reg and isinstance(val, int):
                self.page_cur = val
            self.writes.append((reg, val, line, self.page_cur))
            return 0
        if name.endswith("_write_array") or name.endswith("_read_array"):
            return 0
        if name in ("memcpy", "private_memcpy") and len(args) == 3:
            dst = self.eval(args[0])
            src = self.eval(args[1])
            if isinstance(dst, Ptr) and isinstance(src, Ptr) and dst.loc[0] == "G" and src.loc[0] == "G":
                self.copy_struct(dst.loc[1], src.loc[1], line)
            return UNK
        if name == "ARRAY_SIZE" and len(args) == 1 and args[0][0] == "id":
            n = self.unit.array_len(args[0][1])
            return n if n is not None else UNK
        if name in LOG_FUNCS:
            return UNK
        f = self.unit.funcs.get(name)
        if f is not None and self.depth < 4 and not SKIP_CALLEE.search(name) and callee_relevant(f):
            return self.run_func(name, [self.eval(a) for a in args])
        return UNK

    def copy_struct(self, dst, src, line):
        fields = {}
        m = re.match(r"([A-Za-z_]\w*)(?:\[(\d+)\])?$", src)
        if m:
            flat = self.unit.flat(m.group(1), int(m.group(2)) if m.group(2) else None)
            if flat:
                for k in flat:
                    fields[k] = None
        pre = src + "."
        for k in list(self.store):
            if k.startswith(pre):
                fields[k[len(pre):]] = None
        for f in fields:
            self.store[dst + "." + f] = self.get(("G", src + "." + f))
            self.lines[dst + "." + f] = line
        self.copies[dst] = src

    def table_read(self, reg):
        if self.table is None or not isinstance(reg, int):
            return UNK
        if self.page_reg is not None and reg == self.page_reg:
            # the page register itself: the page set_fps() selected, or else
            # the one the init table leaves the sensor on. The driver now
            # acts on that answer, so later reads are held to that page: a
            # register the table does not write there has its reset value,
            # which the checker does not know.
            page = self.page_cur if self.page_cur is not None else self.table.final_page
            if page is None:
                return UNK
            self.page_cur = page
            return page
        ent = self.table.lookup(reg, self.page_cur)
        return ent[0] if ent is not None else UNK

    # ---- statements ----

    def run_func(self, name, args, top=False):
        f = self.unit.funcs[name]
        ast = f.get_ast()
        self.called.add(name)
        frame = {}
        for i, p in enumerate(f.params):
            frame[p] = args[i] if i < len(args) else UNK
        self.frames.append(frame)
        self.widths.append({})
        self.depth += 1
        ret = None
        try:
            self.exec_block(ast[1], top)
        except ReturnEx as r:
            ret = r.value
        except GotoEx as g:
            # the label is inside a block the jump does not sit in: the rest
            # of this function is not followed, and the caller is told
            self.gotos.append((name, g.label, g.line))
        finally:
            self.depth -= 1
            if top:
                self.top_locals = self.frames[-1]
            self.frames.pop()
            self.widths.pop()
        return ret if ret is not None else UNK

    def exec_block(self, stmts, top=False):
        i = 0
        while i < len(stmts):
            s = stmts[i]
            i += 1
            if top and self.skip_return is not None and s is self.skip_return:
                continue
            try:
                self.exec(s)
            except GotoEx as g:
                # a goto is followed when its label is a statement of this
                # block or of one that encloses it (the error-path and
                # "goto out" idioms); the exception climbs until one has it
                for j, t in enumerate(stmts):
                    if t[0] == "label" and t[1] == g.label:
                        break
                else:
                    raise
                self.jumps += 1
                if self.jumps > 64:
                    raise Abort("goto %s at line %d taken more than 64 times" % (g.label, g.line))
                i = j + 1

    def cond(self, e, node):
        c = self.truth(self.eval(e))
        if c is UNK:
            if mentions(node, GUARDED):
                self.unknown_guard.append(node[-1] if isinstance(node[-1], int) else 0)
            return False
        return c

    def exec(self, s):
        self.steps += 1
        if self.steps > 200000:
            raise Abort("step budget exceeded")
        k = s[0]
        if k == "expr":
            self.eval(s[1])
        elif k == "block":
            self.exec_block(s[1])
        elif k == "if":
            if self.cond(s[1], s):
                self.exec(s[2])
            elif s[3] is not None:
                self.exec(s[3])
        elif k == "decl":
            for name, width, init, aggregate, line in s[1]:
                self.widths[-1][name] = width
                self.frames[-1][name] = UNK
                if init is not None:
                    self.set(("L", name), self.eval(init), line)
                    if self.tainted is not None and len(self.frames) == 1 and mentions(init, self.tainted):
                        self.tainted = self.tainted | frozenset([name])
        elif k == "switch":
            self.exec_switch(s)
        elif k == "return":
            raise ReturnEx(self.eval(s[1]) if s[1] is not None else None)
        elif k == "break":
            raise BreakEx()
        elif k == "continue":
            raise ContinueEx()
        elif k == "goto":
            raise GotoEx(s[1], s[2])
        elif k == "for":
            if s[1] is not None:
                self.exec(s[1])
            for _ in range(256):
                if s[2] is not None and self.truth(self.eval(s[2])) is not True:
                    break
                try:
                    self.exec(s[4])
                except BreakEx:
                    break
                except ContinueEx:
                    pass
                if s[3] is not None:
                    self.eval(s[3])
        elif k == "while":
            for _ in range(256):
                if self.truth(self.eval(s[1])) is not True:
                    break
                try:
                    self.exec(s[2])
                except BreakEx:
                    break
                except ContinueEx:
                    pass
        elif k == "dowhile":
            for _ in range(256):
                try:
                    self.exec(s[1])
                except BreakEx:
                    break
                except ContinueEx:
                    pass
                if self.truth(self.eval(s[2])) is not True:
                    break

    @staticmethod
    def log_only(body, default):
        """A default: branch that does nothing but log, up to its break."""
        for st in body[default + 1:]:
            if st[0] == "break":
                return True
            if st[0] in ("case", "default"):
                continue
            if not _only_logs(st):
                return False
        return True

    def exec_switch(self, s):
        v = self.eval(s[1])
        body = s[2][1] if s[2][0] == "block" else [s[2]]
        if not isinstance(v, (int, Sym)):
            if mentions(s, GUARDED):
                self.unknown_guard.append(s[3])
            return
        start = None
        default = None
        for i, st in enumerate(body):
            if st[0] == "case":
                cv = self.eval(st[1])
                if st[2] is not None:
                    hv = self.eval(st[2])
                    if isinstance(cv, int) and isinstance(hv, int) and isinstance(v, int) and cv <= v <= hv:
                        start = i
                        break
                    continue
                if isinstance(cv, Sym) and isinstance(v, Sym) and cv == v:
                    start = i
                    break
                if isinstance(cv, int) and isinstance(v, int) and cv == v:
                    start = i
                    break
            elif st[0] == "default":
                default = i
        if start is None and default is not None and not self.log_only(body, default):
            start = default
        if start is None:
            # a definite value no case takes, and no default that handles
            # it (none at all, or one that only logs the value as
            # unsupported): this run is not a mode of the driver
            self.nomatch.append((ast_text(s[1]), v, s[3]))
            return
        try:
            for st in body[start + 1:]:
                if st[0] in ("case", "default"):
                    continue
                self.exec(st)
        except BreakEx:
            pass


def _only_logs(st):
    if st[0] in ("empty", "break", "label"):
        return True
    if st[0] == "block":
        return all(_only_logs(x) for x in st[1])
    return (st[0] == "expr" and st[1][0] == "call" and st[1][1][0] == "id"
            and st[1][1][1] in LOG_FUNCS)


def callee_relevant(f, _memo={}):
    if f in _memo:
        return _memo[f]
    names = set(t.text for t in f.body_toks if t.kind == "id")
    res = bool(names & TRACKED) or any(n.endswith("_win_sizes") for n in names)
    _memo[f] = res
    return res


# --------------------------------------------------------------------------
# init tables
# --------------------------------------------------------------------------

PAGE_CANDIDATES = (0xfe, 0xfd, 0xef, 0xff)


class Table(object):
    def __init__(self, name, entries):
        self.name = name
        self.entries = entries     # [(reg, val, line)]
        self.page_reg = None
        self.final = {}
        self.paged = {}
        self.final_page = None
        regs = [e[0] for e in entries]
        self.data16 = any(e[1] > 0xff for e in entries)
        if regs and max(regs) <= 0xff:
            best = 0
            for p in PAGE_CANDIDATES:
                vals = [e[1] for e in entries if e[0] == p]
                if len(vals) >= 2 and len(set(vals)) >= 2 and len(vals) > best:
                    self.page_reg = p
                    best = len(vals)
        page = None
        for reg, val, line in entries:
            if self.page_reg is not None and reg == self.page_reg:
                page = val
                continue
            self.final[reg] = (val, line)
            self.paged[(page, reg)] = (val, line)
        self.final_page = page

    def count(self, reg, page=None):
        n = 0
        cur = None
        for r, v, line in self.entries:
            if self.page_reg is not None and r == self.page_reg:
                cur = v
            elif r == reg and (self.page_reg is None or page is None or cur == page):
                n += 1
        return n

    def lookup(self, reg, page=None):
        if self.page_reg is None:
            return self.final.get(reg)
        if page is not None:
            return self.paged.get((page, reg))
        hits = [v for (p, r), v in self.paged.items() if r == reg]
        if len(hits) == 1:
            return hits[0]
        return self.paged.get((self.final_page, reg))


def build_table(unit, interp, name):
    if name in unit._tables:
        return unit._tables[name]
    g = unit.globs.get(name)
    tab = None
    if g is not None and g.is_array:
        agg = g.agg()
        if agg is not None and agg[0] == "agg":
            entries = []
            for desig, value, line in agg[1]:
                if value[0] != "agg" or len(value[1]) < 2:
                    continue
                rtoks = value[1][0][1][1] if value[1][0][1][0] == "expr" else None
                vtoks = value[1][1][1][1] if value[1][1][1][0] == "expr" else None
                if not rtoks or not vtoks:
                    continue
                rtext = "".join(t.text for t in rtoks)
                if "DELAY" in rtext:
                    continue
                if "END" in rtext:
                    break
                reg = interp.eval_tokens(rtoks)
                val = interp.eval_tokens(vtoks)
                if isinstance(reg, int) and isinstance(val, int):
                    entries.append((reg, val, line))
            tab = Table(name, entries)
    unit._tables[name] = tab
    return tab


# --------------------------------------------------------------------------
# SoC header constants
# --------------------------------------------------------------------------

_CONST_CACHE = {}


def header_consts(root, kver, soc):
    key = (kver, soc)
    if key in _CONST_CACHE:
        return _CONST_CACHE[key]
    consts = {}
    cands = [os.path.join(root, kver, "isp", soc, "include")]
    for other in ("3.10.14", "4.4.94"):
        cands.append(os.path.join(root, other, "isp", soc, "include"))
    cands.append(os.path.join(root, "3.10.14", "isp", "t31", "include"))
    hdr_dir = next((d for d in cands if os.path.isdir(d)), None)
    if hdr_dir is not None:
        for fn in sorted(os.listdir(hdr_dir)):
            if not fn.endswith(".h"):
                continue
            try:
                with open(os.path.join(hdr_dir, fn), "r", encoding="latin-1") as fh:
                    text = strip_comments(fh.read())
            except OSError:
                continue
            for m in re.finditer(r"\benum\b[^{;]*\{([^}]*)\}", text):
                nxt = 0
                for item in m.group(1).split(","):
                    item = item.strip()
                    if not item:
                        continue
                    mm = re.match(r"(\w+)\s*(?:=\s*(.+))?$", item, re.S)
                    if not mm:
                        continue
                    if mm.group(2) is not None:
                        v = const_eval(mm.group(2), consts)
                        if v is None:
                            continue
                        nxt = v
                    consts.setdefault(mm.group(1), nxt)
                    nxt += 1
            for m in re.finditer(r"^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+(.+)$", text, re.M):
                v = const_eval(m.group(2), consts)
                if v is not None:
                    consts.setdefault(m.group(1), v)
    _CONST_CACHE[key] = consts
    return _CONST_CACHE[key]


class _ConstUnit(object):
    defines = {}
    globs = {}
    funcs = {}


def const_eval(text, consts):
    it = Interp(_ConstUnit, consts, None)
    try:
        v = it.eval_tokens(tokenize(text))
    except Exception:
        return None
    return v if isinstance(v, int) else None


# --------------------------------------------------------------------------
# per-driver analysis
# --------------------------------------------------------------------------

MODE_PRIORITY = ("@default_boot", "sensor_max_fps", "data_interface", "sensor_resolution",
                 "data_type", "wdr", "@video_interface")


class Finding(object):
    def __init__(self, severity, check, rel, line, mode, message):
        self.severity = severity
        self.check = check
        self.rel = rel
        self.line = line
        self.mode = mode
        self.message = message

    def as_dict(self):
        parts = self.rel.replace("\\", "/").split("/")
        return {"severity": self.severity, "check": self.check,
                "driver": os.path.splitext(parts[-1])[0],
                "soc": parts[-2] if len(parts) > 1 else "",
                "kernel": parts[0] if len(parts) > 2 else "",
                "file": self.rel, "line": self.line, "mode": self.mode,
                "message": self.message}


def collect_mode_vars(unit, entry_funcs, it):
    """Static scan: which module parameters / info fields do the mode
    functions switch on, and against which constants."""
    cands = {}
    seen = set()

    def modekey(e, aliases):
        if e[0] == "cast":
            return modekey(e[2], aliases)
        if e[0] == "id":
            if e[1] in aliases:
                return aliases[e[1]]
            if e[1] in unit.module_params:
                return e[1]
            return None
        if e[0] == "mem":
            txt = ast_text(e)
            m = INFO_RE.search(txt)
            if m:
                return "@" + m.group(1)
            base = e[1]
            if base[0] == "id" and base[1] in aliases and aliases[base[1]] == "@":
                return "@" + e[2]
        return None

    def add(key, expr):
        v = it.eval(expr)
        if isinstance(v, (int, Sym)):
            cands.setdefault(key, []).append((ast_text(expr), v))

    def walk(node, aliases, fname):
        if isinstance(node, list):
            for x in node:
                walk(x, aliases, fname)
            return
        if not isinstance(node, tuple) or not node:
            return
        k = node[0]
        if k == "decl":
            for name, width, init, aggregate, line in node[1]:
                if init is not None:
                    mk = modekey(init, aliases)
                    if mk:
                        aliases[name] = mk
                    elif init[0] == "un" and init[1] == "&" and ast_text(init[2]).endswith("info"):
                        aliases[name] = "@"
                    walk(init, aliases, fname)
            return
        if k == "assign" and node[1] == "=" and node[2][0] == "id":
            mk = modekey(node[3], aliases)
            if mk:
                aliases[node[2][1]] = mk
        if k == "switch":
            mk = modekey(node[1], aliases)
            if mk:
                body = node[2][1] if node[2][0] == "block" else [node[2]]
                for st in body:
                    if st[0] == "case":
                        add(mk, st[1])
                        if st[2] is not None:
                            lo, hi = it.eval(st[1]), it.eval(st[2])
                            if isinstance(lo, int) and isinstance(hi, int) and 0 < hi - lo <= 16:
                                for x in range(lo + 1, hi + 1):
                                    cands.setdefault(mk, []).append((str(x), x))
        if k == "bin" and node[1] == "==":
            for a, b in ((node[2], node[3]), (node[3], node[2])):
                mk = modekey(a, aliases)
                if mk:
                    add(mk, b)
        if k == "call" and node[1][0] == "id":
            f = unit.funcs.get(node[1][1])
            if f is not None and not SKIP_CALLEE.search(f.name) and callee_relevant(f):
                sub = {}
                for i, a in enumerate(node[2]):
                    mk = modekey(a, aliases)
                    if mk and i < len(f.params):
                        sub[f.params[i]] = mk
                scan(f.name, sub)
        for x in node[1:]:
            if isinstance(x, (tuple, list)):
                walk(x, aliases, fname)

    def scan(fname, aliases):
        key = (fname, tuple(sorted(aliases.items())))
        if key in seen:
            return
        seen.add(key)
        f = unit.funcs.get(fname)
        if f is None:
            return
        walk(f.get_ast(), dict(aliases), fname)

    for fn in entry_funcs:
        scan(fn, {})
    return cands


def fmt_fps(fps):
    if not isinstance(fps, int) or (fps & 0xffff) == 0:
        return "?"
    num, den = fps >> 16, fps & 0xffff
    if den == 1:
        return "%d" % num
    return "%.4g" % (num / float(den))


def near(a, b, rel=0.005, absol=2):
    return abs(a - b) <= max(absol, rel * max(abs(a), abs(b)))


class DriverResult(object):
    def __init__(self, rel):
        self.rel = rel
        self.status = "ok"         # ok | no-modes | error
        self.reason = ""
        self.modes = []
        self.findings = []
        self.notes = []
        self.skipped = []          # parameter combinations left out as implausible
        self.left_out = {}
        self.md5 = ""


def analyze(root, path, rel):
    res = DriverResult(rel)
    try:
        with open(path, "rb") as fh:
            raw = fh.read()
    except OSError as e:
        res.status = "error"
        res.reason = "cannot read: %s" % e
        return res
    res.md5 = hashlib.md5(raw).hexdigest()
    _MENTIONS.clear()
    text = raw.decode("latin-1")
    parts = rel.replace("\\", "/").split("/")
    kver = parts[0] if len(parts) > 2 else ""
    soc = parts[-2] if len(parts) > 1 else ""
    predefined = {}
    if kver.startswith("4.4"):
        predefined["CONFIG_KERNEL_4_4_94"] = ("1", 0)
    try:
        unit = Unit(path, rel, text, predefined)
        for line, cond, why in unit.unread_conditions:
            res.notes.append("line %d: #if %s is not read (%s): the block is taken as false"
                             % (line, cond, why))
        _analyze_unit(unit, header_consts(root, kver, soc), res)
    except ParseError as e:
        res.status = "error"
        res.reason = "parse: %s" % e
    except Abort as e:
        res.status = "error"
        res.reason = str(e)
    except RecursionError:
        res.status = "error"
        res.reason = "recursion limit"
    return res


def pick_func(unit, suffix, prefer=None):
    names = [n for n in unit.funcs if n.endswith(suffix)]
    if prefer and prefer + suffix in unit.funcs:
        return prefer + suffix
    if "sensor" + suffix in unit.funcs:
        return "sensor" + suffix
    return sorted(names, key=len)[0] if names else None


def first_dead_return(ast):
    """A top-level unconditional return that is not the last statement."""
    stmts = ast[1]
    for i, s in enumerate(stmts[:-1]):
        if s[0] == "return":
            return s
    return None


def find_th_assign(node, out):
    if isinstance(node, list):
        for x in node:
            find_th_assign(x, out)
    elif isinstance(node, tuple) and node:
        if node[0] == "assign" and node[2][0] == "mem" and node[2][2] == "total_height":
            out.append(node)
        for x in node[1:]:
            if isinstance(x, (tuple, list)):
                find_th_assign(x, out)


def expr_ids(e, out):
    if isinstance(e, tuple) and e:
        if e[0] == "id":
            out.append(e[1])
        for x in e[1:]:
            if isinstance(x, (tuple, list)):
                expr_ids(x, out)
    elif isinstance(e, list):
        for x in e:
            expr_ids(x, out)


def soi_indirect(writes):
    """SOI (jx*) group write: reg 0xc0+2n takes an address, 0xc1+2n its value."""
    out = []
    i = 0
    while i < len(writes):
        w = writes[i]
        if (i + 1 < len(writes) and isinstance(w[0], int) and 0xc0 <= w[0] <= 0xce
                and w[0] % 2 == 0 and isinstance(w[1], int) and writes[i + 1][0] == w[0] + 1):
            nxt = writes[i + 1]
            out.append((w[1], nxt[1], nxt[2], nxt[3]))
            i += 2
            continue
        out.append(w)
        i += 1
    return out


def _analyze_unit(unit, consts, res):
    rel = unit.rel
    attr_g = unit.find_glob("tx_isp_sensor_attribute", False)
    win_g = unit.find_glob("tx_isp_sensor_win_setting", True)
    if attr_g is None or win_g is None:
        res.status = "no-modes"
        res.reason = "no tx_isp_sensor_attribute / tx_isp_sensor_win_setting[] found"
        return
    attr = attr_g.name
    win = win_g.name
    set_fps = pick_func(unit, "_set_fps")
    prefix = set_fps[:-len("_set_fps")] if set_fps else None
    probe = pick_func(unit, "_probe", prefix)
    attr_check = pick_func(unit, "_attr_check", prefix)
    init = None
    for cand in ([prefix + "_init"] if prefix else []) + ["sensor_init"]:
        if cand in unit.funcs and len(unit.funcs[cand].params) >= 2:
            init = cand
            break
    entries = [f for f in (probe, attr_check, init) if f]
    if not entries:
        res.status = "no-modes"
        res.reason = "no probe / attr_check / init function found"
        return

    base = Interp(unit, consts, attr)
    cands = collect_mode_vars(unit, entries + ([set_fps] if set_fps else []), base)

    # candidate values per mode variable; the module parameter's default too
    variables = []
    for key in sorted(cands, key=lambda k: (MODE_PRIORITY.index(k) if k in MODE_PRIORITY else 99, k)):
        vals = []
        labels = {}
        for label, v in cands[key]:
            if v in labels:
                if label != labels[v] and label not in labels.get(("dup", v), []):
                    labels.setdefault(("dup", v), [labels[v]]).append(label)
                continue
            labels[v] = label
            vals.append((label, v))
        if not key.startswith("@"):
            dv = base.ident(key)
            if isinstance(dv, (int, Sym)) and dv not in labels:
                vals.append((str(dv), dv))
        dups = [v for k, v in labels.items() if isinstance(k, tuple)]
        variables.append((key, vals, dups))
    total = 1
    kept = []
    for key, vals, dups in variables:
        if total * max(1, len(vals)) > 512:
            res.notes.append("mode variable %s not enumerated (too many combinations)" % key)
            continue
        total *= max(1, len(vals))
        kept.append((key, vals, dups))

    for key, vals, dups in kept:
        for labs in dups:
            if all(re.search(r"FPS|BOOT|INTERFACE|RES", l) for l in labs):
                res.findings.append(Finding(
                    "medium", "dup-enum", rel, unit.funcs[entries[0]].line, key,
                    "%s is compared with %s, which have the same value here: the "
                    "branch for the later one cannot be selected"
                    % (key.lstrip("@"), " and ".join(labs))))

    def envs(i, cur):
        if i == len(kept):
            yield list(cur)
            return
        key, vals, dups = kept[i]
        if not vals:
            for x in envs(i + 1, cur):
                yield x
            return
        for label, v in vals:
            cur.append((key, label, v))
            for x in envs(i + 1, cur):
                yield x
            cur.pop()

    seen_modes = {}
    skipped_combos = 0
    lost_gotos = {}
    aborted = {}
    rejected = 0
    for env in envs(0, []):
        it = base.clone()
        for key, label, v in env:
            it.store[key] = v
        ok = True
        try:
            for fn in entries:
                if fn in it.called and fn != probe:
                    continue
                seen = len(it.gotos)
                ret = it.run_func(fn, [])
                if fn not in (probe, attr_check):
                    continue
                if len(it.gotos) > seen:
                    lost_gotos[it.gotos[-1]] = lost_gotos.get(it.gotos[-1], 0) + 1
                    ok = False
                    break
                if isinstance(ret, int) and ret != 0:
                    rejected += 1       # the driver refuses this combination itself
                    ok = False
                    break
        except Abort as e:
            aborted[str(e)] = aborted.get(str(e), 0) + 1
            ok = False
        if not ok:
            continue
        if it.nomatch:
            rejected += 1           # a switch has no case for the value
            continue
        facts = mode_facts(unit, it, attr, win)
        if facts is None:
            continue
        if implausible(env, facts):
            skipped_combos += 1
            res.skipped.append({
                "params": ",".join("%s=%s" % (k.lstrip("@"), lab) for k, lab, _ in env),
                "table": facts.get("table"),
                "why": "interface-table"})
            continue
        key = json.dumps(facts, sort_keys=True, default=str)
        if key in seen_modes:
            seen_modes[key]["envs"].append(env)
            continue
        seen_modes[key] = {"facts": facts, "envs": [env], "interp": it}

    if skipped_combos:
        res.notes.append("%d parameter combinations left out: a DVP init table with a MIPI "
                         "interface setting or the reverse (listed in the JSON output)" % skipped_combos)
    res.left_out = {"interface-table": skipped_combos, "refused": rejected,
                    "goto": sum(lost_gotos.values()), "aborted": sum(aborted.values())}
    if rejected:
        res.notes.append("%d parameter combinations left out: the driver refuses them (probe or "
                         "attr_check returns an error, or a switch has no case for the value)"
                         % rejected)
    for (fn, label, line), n in sorted(lost_gotos.items()):
        res.notes.append("%d parameter combinations not analysed: %s() jumps to label %s (line %d), "
                         "which is inside another block; such a jump is not followed" % (n, fn, label, line))
    for why, n in sorted(aborted.items()):
        res.notes.append("%d parameter combinations not analysed: %s" % (n, why))
    if not seen_modes:
        res.status = "no-modes"
        res.reason = "no mode could be resolved from %s" % ", ".join(entries)
        return

    # a mode variable matters only if it tells two modes apart
    for m in seen_modes.values():
        m["label"] = mode_label(m["envs"], kept, len(seen_modes))
    for m in sorted(seen_modes.values(), key=lambda m: m["label"]):
        m["facts"]["mode"] = m["label"]
        check_mode(unit, m, attr, set_fps, res)
        res.modes.append(m["facts"])
    if not set_fps:
        res.notes.append("no set_fps function: only static checks ran")


def implausible(env, facts):
    """A DVP-named init table selected together with a MIPI interface
    setting (or the reverse): a combination nobody would configure."""
    table = (facts.get("table") or "").lower()
    t_dvp = "dvp" in table
    t_mipi = "mipi" in table
    if t_dvp == t_mipi:
        return False
    for key, label, v in env:
        if "interface" not in key.lower():
            continue
        lab = label.upper()
        if t_dvp and "MIPI" in lab:
            return True
        if t_mipi and ("DVP" in lab or "BT" in lab):
            return True
    return False


def mode_label(envs, kept, nmodes):
    if nmodes == 1 or not envs:
        return "-"
    parts = []
    for i, (key, label, v) in enumerate(envs[0]):
        vals = set(str(e[i][2]) for e in envs)
        allvals = len(kept[i][1]) if i < len(kept) else 0
        if len(vals) == allvals and allvals > 1:
            continue        # the mode exists for every value: not a selector
        labs = []
        for e in envs:
            lab = re.sub(r"^TX_SENSOR_(MAX_FPS_|DATA_INTERFACE_)?", "", e[i][1])
            if lab not in labs:
                labs.append(lab)
        parts.append("%s=%s" % (key.lstrip("@"), "|".join(labs)))
    return ",".join(parts) if parts else "-"


def static_int(unit, it, key):
    probe = Interp(unit, it.consts, it.attr_name)
    v = probe.static_value(key)
    return v if isinstance(v, int) else None


def mode_facts(unit, it, attr, win):
    cur = it.cur_win
    if cur is None:
        for name in ("wsize",):
            v = it.get(("G", name)) if name in unit.globs else None
            if isinstance(v, Ptr):
                cur = v.loc[1]
    if cur is None:
        cur = "%s[0]" % win

    def g(key):
        v = it.get(("G", key))
        return v if isinstance(v, (int, float)) else (str(v) if isinstance(v, (Sym, Ptr)) else None)

    regs = it.get(("G", cur + ".regs"))
    table = regs.loc[1] if isinstance(regs, Ptr) and regs.loc[0] == "G" else None
    f = {
        "win": cur,
        "table": table,
        "width": g(cur + ".width"),
        "height": g(cur + ".height"),
        "fps": g(cur + ".fps"),
        "fps_static": static_int(unit, it, cur + ".fps") if (cur + ".fps") in it.dyn
        and (cur + ".regs") not in it.dyn else None,
        "mbus_code": g(cur + ".mbus_code"),
        "total_width": g(attr + ".total_width"),
        "total_height": g(attr + ".total_height"),
        "max_integration_time": g(attr + ".max_integration_time"),
        "max_integration_time_native": g(attr + ".max_integration_time_native"),
        "integration_time_limit": g(attr + ".integration_time_limit"),
        "one_line_expr_in_us": g(attr + ".one_line_expr_in_us"),
        "dbus_type": g(attr + ".dbus_type"),
        "data_type": g(attr + ".data_type"),
        "mipi_src": it.copies.get(attr + ".mipi"),
        "image_twidth": g(attr + ".mipi.image_twidth"),
        "image_theight": g(attr + ".mipi.image_theight"),
        "crop": [g(attr + ".mipi.mipi_sc.mipi_crop_start0x"), g(attr + ".mipi.mipi_sc.mipi_crop_start0y")],
        "unknown_guard": sorted(set(it.unknown_guard)),
        "lines": {
            "total_height": it.lines.get(attr + ".total_height"),
            "total_width": it.lines.get(attr + ".total_width"),
            "max_integration_time": it.lines.get(attr + ".max_integration_time"),
            "win": it.lines.get(cur + ".fps") or unit.globs[cur.split("[")[0]].line,
        },
    }
    return f


def run_set_fps(unit, m, set_fps, fps, inject=None, inject_var=None, skip=None):
    it = m["interp"].clone()
    f = unit.funcs[set_fps]
    table = build_table(unit, it, m["facts"]["table"]) if m["facts"]["table"] else None
    it.table = table
    it.page_reg = table.page_reg if table is not None else None
    it.inject_val = inject
    it.inject_var = inject_var
    params = f.params
    it.inject_param = frozenset([params[1]]) if len(params) > 1 else frozenset(["fps"])
    it.tainted = it.inject_param
    it.skip_return = skip
    it.lines.pop(it.attr_name + ".total_height", None)
    args = [UNK, fps] + [UNK] * max(0, len(params) - 2)
    ret = it.run_func(set_fps, args, top=True)
    mask = 0xffff if table is not None and table.data16 else 0xff
    writes = [(w[0], (w[1] & mask) if isinstance(w[1], int) else w[1], w[2], w[3]) for w in it.writes]
    if os.path.basename(unit.rel).startswith("jx"):
        writes = soi_indirect(writes)
    assigned = (it.attr_name + ".total_height") in it.lines
    th = it.store.get(it.attr_name + ".total_height") if assigned else None
    return {
        "ret": ret,
        "writes": writes,
        "th": th if isinstance(th, int) else None,
        "assigned": assigned,
        "locals": it.top_locals or {},
        "injected": it.injected,
        "table": table,
        "reads": it.reads,
        "no_table": table is None,
        "gotos": list(it.gotos),
    }


def write_map(run):
    out = {}
    for reg, val, line, page in run["writes"]:
        if isinstance(reg, int):
            out[(page, reg)] = (val, line)
    return out


def table_frame(unit, m, set_fps, fps, inject_var, skip, base):
    """Find the frame length the init table programs, by inverting the
    writes of set_fps(). Returns (T, detail dict) or (None, reason)."""
    probes = (base, base + 1, base + 256, base + 1000, base + 65536)
    runs = {}
    for v in probes:
        r = run_set_fps(unit, m, set_fps, fps, inject=v, inject_var=inject_var, skip=skip)
        if not r["injected"]:
            return None, "set_fps() does not compute the frame length from fps in a way the checker follows"
        runs[v] = r
    maps = dict((v, write_map(runs[v])) for v in probes)
    keys = set()
    for v in probes:
        keys.update(maps[v].keys())
    pos = {}
    for i, w in enumerate(runs[base]["writes"]):
        if isinstance(w[0], int):
            pos[(w[3], w[0])] = i
    dep = []
    for k in sorted(keys, key=lambda x: pos.get(x, 999)):
        vals = [maps[v].get(k, (None, 0))[0] for v in probes]
        if any(not isinstance(x, int) for x in vals):
            if any(x != vals[0] for x in vals):
                return None, "set_fps() writes a value the checker cannot evaluate to reg 0x%x" % k[1]
            continue
        if len(set(vals)) > 1:
            dep.append(k)
    if not dep:
        return None, "set_fps() writes no frame-length register directly"
    # the first registers written, one per byte rank, are the frame length;
    # later ones (copies, values derived from it) are left alone
    group = {}
    for k in dep:
        a = dict((v, maps[v][k][0]) for v in probes)
        if a[base] != a[base + 1]:
            r = 0
        elif a[base] != a[base + 256]:
            r = 1
        elif a[base] != a[base + 65536]:
            r = 2
        else:
            continue
        if r in group:
            break
        group[r] = k
    if 0 not in group:
        return None, "frame-length register layout not understood (regs %s)" % \
            ", ".join("0x%x" % x[1] for x in dep)
    rank = dict((k, r) for r, k in group.items())
    dep = list(rank)
    table = runs[base]["table"]
    if table is None:
        return None, "init table not found"
    tvals = {}
    for k in dep:
        ent = table.lookup(k[1], k[0])
        if ent is None:
            if rank[k] == 2:
                tvals[k] = (0, None)
                continue
            return None, "init table %s does not write frame-length reg 0x%x" % (table.name, k[1])
        n = table.count(k[1], k[0])
        if n > 3:
            return None, "reg 0x%x is written %d times in the init table: an indirect data " \
                "register, not followed" % (k[1], n)
        tvals[k] = ent

    def combine(get):
        return sum(get(k) << (8 * rank[k]) for k in dep)

    c1 = combine(lambda k: maps[base][k][0])
    c2 = combine(lambda k: maps[base + 1000][k][0])
    if c2 == c1:
        return None, "frame-length writes do not follow the frame length"
    slope = (c2 - c1) / 1000.0

    def solve(target):
        ct = combine(lambda k: target[k][0])
        guess = int(round(base + (ct - c1) / slope))
        for v in [guess] + [guess + d for d in (-1, 1, -2, 2, -3, 3)]:
            if v <= 0:
                continue
            r = run_set_fps(unit, m, set_fps, fps, inject=v, inject_var=inject_var, skip=skip)
            wm = write_map(r)
            if all(wm.get(k, (None, 0))[0] == target[k][0] for k in dep if target[k][1] is not None):
                return r
        return None

    r = solve(tvals)
    if r is None:
        return None, "init table's frame-length bytes cannot be produced by set_fps() (regs %s)" % \
            ", ".join("0x%x=0x%02x" % (k[1], tvals[k][0]) for k in dep)
    if r["th"] is None:
        return None, "set_fps() does not set total_height"
    order = sorted(dep, key=lambda k: -rank[k])
    detail = {
        "regs": ["0x%x" % k[1] for k in order],
        "bytes": ["0x%02x" % tvals[k][0] for k in order],
        "line": tvals[group[0]][1],
        "table": table.name,
        "how": "written",
        "swapped": None,
    }
    if len(dep) == 2 and 1 in group and not table.data16:
        sw = {group[0]: tvals[group[1]], group[1]: tvals[group[0]]}
        rs = solve(sw)
        if rs is not None and rs["th"] is not None:
            detail["swapped"] = rs["th"]
    return r["th"], detail


def table_frame_readback(unit, m, set_fps, fps, skip, r0):
    """set_fps() that changes the rate some other way (hts) and reports the
    frame length it reads back from the sensor: with reads answered from
    the init table, that is the table's frame length."""
    if not r0["assigned"] or r0["th"] is None or r0["no_table"]:
        return None, None
    it = m["interp"].clone()
    it.skip_return = skip
    key = it.attr_name + ".total_height"
    it.lines.pop(key, None)
    it.run_func(set_fps, [UNK, fps], top=True)     # no table: every read is unknown
    if key in it.lines and isinstance(it.store.get(key), int):
        return None, None       # a constant, not a read
    regs = [r for r in r0["reads"] if isinstance(r[0], int) and isinstance(r[1], int)]
    if not regs:
        return None, None
    half = ((fps >> 16) << 16) | ((fps & 0xffff) * 2)
    r2 = run_set_fps(unit, m, set_fps, half, skip=skip)
    if not r2["assigned"] or r2["th"] != r0["th"]:
        return None, None       # it follows the rate: arithmetic, not a read
    table = r0["table"]
    ent = table.lookup(regs[-1][0]) if table is not None else None
    detail = {
        "regs": ["0x%x" % r[0] for r in regs[-2:]],
        "bytes": ["0x%02x" % r[1] for r in regs[-2:]],
        "line": ent[1] if ent is not None else None,
        "table": table.name if table is not None else "?",
        "how": "read back",
    }
    return r0["th"], detail


def check_mode(unit, m, attr, set_fps, res):
    f = m["facts"]
    rel = unit.rel
    label = m["label"]
    fps = f["fps"]
    # a total_height of 0 is a mode the driver left unfilled: nothing to compare
    A = f["total_height"] if isinstance(f["total_height"], int) and f["total_height"] > 0 else None
    W = f["total_width"] if isinstance(f["total_width"], int) else None
    ln = f["lines"]
    f["notes"] = []

    guard_txt = ""
    if f.get("unknown_guard"):
        guard_txt = " [verify by hand: the checker could not evaluate the condition at line %s around "             "the mode's total_height]" % ", ".join(str(x) for x in f["unknown_guard"])

    def add(sev, check, line, msg):
        res.findings.append(Finding(sev, check, rel, line or 0, label, msg + guard_txt))

    nominal = None
    if isinstance(fps, int) and (fps & 0xffff) and (fps >> 16):
        nominal = (fps >> 16) / float(fps & 0xffff)

    # ---- static checks ----
    if A and isinstance(f["height"], int) and A < f["height"]:
        add("high", "frame-short", ln["total_height"],
            "total_height %d is less than the image height %d of window %s" % (A, f["height"], f["win"]))
    if A:
        for k in ("max_integration_time", "max_integration_time_native", "integration_time_limit"):
            v = f[k]
            if isinstance(v, int) and v > A and not (2 * A - 64 <= v <= 2 * A):
                add("medium", "integration-limit", ln["max_integration_time"] or ln["total_height"],
                    "%s %d is longer than total_height %d (and is not the 2 x total_height of a "
                    "half-line exposure)" % (k, v, A))
                break
    tw, th_ = f["image_twidth"], f["image_theight"]
    if isinstance(tw, int) and isinstance(th_, int) and tw and th_ \
            and isinstance(f["width"], int) and isinstance(f["height"], int) \
            and (tw, th_) != (0, 0):
        crop = [c for c in f["crop"] if isinstance(c, int) and c]
        if (tw, th_) != (f["width"], f["height"]) and not crop:
            sev = "medium" if (tw < f["width"] or th_ < f["height"]) else "low"
            add(sev, "image-size", ln["win"],
                "window %s is %dx%d but the mipi bus copied from %s states image %dx%d (no crop offsets set)"
                % (f["win"], f["width"], f["height"], f["mipi_src"] or "the attr", tw, th_))
    if f["table"]:
        mname = re.search(r"(\d{3,4})_(\d{3,4})(?:_(\d+)fps)?", f["table"])
        if mname:
            w_, h_ = int(mname.group(1)), int(mname.group(2))
            msgs = []
            if isinstance(f["width"], int) and isinstance(f["height"], int) and (w_, h_) != (f["width"], f["height"]):
                msgs.append("size %dx%d against the window's %dx%d" % (w_, h_, f["width"], f["height"]))
            if mname.group(3) and nominal and abs(int(mname.group(3)) - nominal) > 0.5:
                msgs.append("%s fps against the window's %s" % (mname.group(3), fmt_fps(fps)))
            if msgs:
                add("low", "table-name", ln["win"], "init table %s is named for %s" % (f["table"], "; ".join(msgs)))

    if not set_fps or nominal is None:
        f["T"] = f["C"] = None
        return

    # ---- set_fps at the nominal rate ----
    ast = unit.funcs[set_fps].get_ast()
    r0 = run_set_fps(unit, m, set_fps, fps)
    skip = None
    dead = False
    if not r0["assigned"]:
        d = first_dead_return(ast)
        if d is not None:
            r1 = run_set_fps(unit, m, set_fps, fps, skip=d)
            if r1["assigned"]:
                r0, skip, dead = r1, d, True
    f["set_fps_dead"] = dead
    for fn, lab, line in r0["gotos"]:
        f["notes"].append("%s() jumps to label %s (line %d), which is inside another block: "
                          "the code after the jump was not followed" % (fn, lab, line))
    C = r0["th"] if r0["assigned"] else None
    loc = r0["locals"]
    sclk = None
    for name in ("sclk", "wpclk", "pclk", "clk", "sensor_sclk"):
        if isinstance(loc.get(name), int) and loc.get(name) > 100000:
            sclk = loc[name]
            break
    hts = loc.get("hts") if isinstance(loc.get("hts"), int) and loc.get("hts") else None
    f["sclk"], f["hts"] = sclk, hts
    f["C"] = C
    if not r0["assigned"] and isinstance(r0["ret"], int) and r0["ret"] != 0 and not r0["writes"]:
        mx = loc.get("max_fps")
        add("medium", "fps-range", unit.funcs[set_fps].line,
            "set_fps() rejects the mode's own rate of %s fps (window %s)%s"
            % (fmt_fps(fps), f["win"], ", max_fps %s" % mx if isinstance(mx, int) else ""))

    # ---- the init table's frame length ----
    T = None
    detail = None
    ths = []
    find_th_assign(ast, ths)
    inject_var = None
    if ths:
        ids = []
        expr_ids(ths[0][3], ids)
        fn_locals = set(loc.keys())
        for name in ids:
            if name in fn_locals:
                inject_var = name
                break
    if inject_var and f["table"]:
        base = A if A and A > 16 else (C if C and C > 16 else 2000)
        T, detail = table_frame(unit, m, set_fps, fps, inject_var, skip, base)
        if T is None:
            f["T_reason"] = detail
            detail = None
            if f["T_reason"].startswith("set_fps() does not compute"):
                T, detail = table_frame_readback(unit, m, set_fps, fps, skip, r0)
                if T is not None:
                    C = None
                    f["C"] = None
                    f["T_reason"] = None
    else:
        f["T_reason"] = "set_fps() does not set total_height from a local" if f["table"] else "init table not resolved"
    if T is not None and T <= 0:
        f["T_reason"] = "the init table's frame-length registers hold %d" % T
        T = detail = None
    f["T"] = T
    if detail:
        f["T_regs"] = detail["regs"]

    dead_txt = " (set_fps() returns before this code; it is read for the arithmetic only)" if dead else ""
    # A frame-sync mode stretches total_height and lowers fps by the same
    # factor (the sensor is triggered from outside); compare at the rate the
    # window setting was written for.
    scale = 1.0
    scale_txt = ""
    fs = f.get("fps_static")
    if isinstance(fs, int) and fs != fps and (fs & 0xffff) and (fs >> 16):
        scale = nominal / ((fs >> 16) / float(fs & 0xffff))
        scale_txt = " (the mode code rescales the window's %s fps to %s fps; compared at %s fps)" % (
            fmt_fps(fs), fmt_fps(fps), fmt_fps(fs))
    Ae = int(round(A * scale)) if A is not None else None
    Ce = int(round(C * scale)) if C is not None else None
    f["A_eq"], f["C_eq"] = Ae, Ce
    if A is None:
        a_txt = "?"
    elif scale == 1.0:
        a_txt = "%d" % A
    else:
        a_txt = "%d (%d at the window's rate)" % (A, Ae)
    swapped = detail.get("swapped") if detail else None
    if (T is not None and Ae is not None and swapped is not None and not near(T, Ae)
            and (T > 3 * Ae or T * 3 < Ae) and near(swapped, Ae, rel=0.05)):
        # T as set_fps() would decode the table is meaningless here; report
        # the frame the table holds, read in the table's own byte order
        f["T_as_set_fps_reads"] = T
        f["T"] = swapped
        f["notes"].append("T is read in the init table's byte order; in set_fps()'s order the same "
                          "bytes are %d" % T)
        add("high", "byte-order", unit.funcs[set_fps].line,
            "set_fps() writes the frame length's high byte to reg %s and its low byte to reg %s, "
            "but init table %s holds them the other way round (%s = %s, line %s: %d lines read the "
            "table's way, total_height is %s): a set_fps() call programs a wrong frame length%s"
            % (detail["regs"][0], detail["regs"][1], detail["table"], "/".join(detail["regs"]),
               "/".join(detail["bytes"]), detail["line"], swapped, a_txt, dead_txt))
    elif T is not None and Ae is not None and not near(T, Ae):
        where = "init table %s programs a %d-line frame (regs %s %s = %s, line %s)" % (
            detail["table"], T, "/".join(detail["regs"]), detail["how"], "/".join(detail["bytes"]),
            detail["line"])
        pct = 100.0 * (Ae - T) / T
        if Ce is not None and near(Ce, T):
            add("high", "frame-length", ln["total_height"],
                "%s but the mode reports total_height %s; sclk/hts at %s fps also gives %d: "
                "the ISP's line time is %.1f%% %s%s%s"
                % (where, a_txt, fmt_fps(fps), C, abs(pct), "short" if pct > 0 else "long",
                   scale_txt, dead_txt))
        elif Ce is not None and near(Ce, Ae):
            add("medium", "frame-length", ln["total_height"],
                "%s but the mode reports total_height %s, which is what sclk/hts gives at %s fps: "
                "the sensor starts at about %.2f fps until set_fps() runs%s%s"
                % (where, a_txt, fmt_fps(fps), nominal * Ae / T / scale, scale_txt, dead_txt))
        else:
            add("high", "frame-length", ln["total_height"],
                "%s but the mode reports total_height %s%s%s%s"
                % (where, a_txt,
                   "; sclk/hts at %s fps gives %d, a third value" % (fmt_fps(fps), C) if C is not None else
                   "; set_fps() gives no third opinion", scale_txt, dead_txt))
    elif C is not None and A is not None and not near(C, A, rel=0.01):
        if T is not None:
            what = "the init table and total_height agree on %d lines" % A
        else:
            what = "the mode reports total_height %d" % A
        add("medium" if not dead and not near(C, A, rel=0.02) else "low", "fps-clock", ln["total_height"],
            "%s, but set_fps() at the mode's %s fps computes %d (sclk %s, hts %s): "
            "the sclk constant, the hts or the fps label is off by %.1f%%%s"
            % (what, fmt_fps(fps), C, sclk if sclk else "?", hts if hts else "?",
               100.0 * abs(C - A) / A, dead_txt))

    # ---- softer checks on the arithmetic ----
    mx = loc.get("max_fps")
    if isinstance(mx, int) and 0 < mx <= 240 and sclk and hts and isinstance(f["height"], int) and not dead:
        vmin = sclk // hts // mx
        if vmin < f["height"] and mx > nominal + 0.5:
            add("low", "max-fps", unit.funcs[set_fps].line,
                "set_fps() accepts up to %d fps in this mode, where it would program a %d-line frame "
                "for a %d-line image (sclk %d, hts %d)" % (mx, vmin, f["height"], sclk, hts))
    one = f["one_line_expr_in_us"]
    if isinstance(one, int) and one and sclk and hts:
        real = hts * 1e6 / sclk
        if abs(real - one) > 1.5 and abs(real - one) / real > 0.1:
            add("low", "line-time", ln["total_height"],
                "one_line_expr_in_us is %d but hts / sclk = %d / %d is %.1f us" % (one, hts, sclk, real))
    if W and hts and not near(W, hts, rel=0.01):
        add("low", "total-width", ln["total_width"],
            "total_width is %d but set_fps() works with hts %d" % (W, hts))


# --------------------------------------------------------------------------
# command line
# --------------------------------------------------------------------------

def find_drivers(root, patterns, follow_links=False):
    """The driver files to check, and the SoC folders left out because they
    are symbolic links to another folder (the same sources under a second
    name)."""
    out = []
    aliases = []
    for kver in sorted(os.listdir(root)):
        sdir = os.path.join(root, kver, "sensor-src")
        if not os.path.isdir(sdir):
            continue
        for soc in sorted(os.listdir(sdir)):
            d = os.path.join(sdir, soc)
            if not os.path.isdir(d):
                continue
            if os.path.islink(d) and not follow_links:
                aliases.append("%s/sensor-src/%s -> %s" % (kver, soc, os.readlink(d)))
                continue
            for fn in sorted(os.listdir(d)):
                if not fn.endswith(".c"):
                    continue
                rel = "/".join((kver, "sensor-src", soc, fn))
                if patterns:
                    stem = fn[:-2]
                    if not any(fnmatch.fnmatch(stem, p) or fnmatch.fnmatch(rel, p)
                               or fnmatch.fnmatch(soc + "/" + stem, p) for p in patterns):
                        continue
                out.append((os.path.join(d, fn), rel))
    return out, aliases


def tree_identity(root):
    """The commit the drivers were read from, when root is the top of a git
    work tree; None otherwise (a tarball, a subdirectory, no git). "dirty"
    says that a tracked file under the kernel-version folders, where the
    drivers and their headers are, differs from that commit."""
    def git(*args):
        try:
            r = subprocess.run(("git", "-C", root) + args, stdout=subprocess.PIPE,
                               stderr=subprocess.DEVNULL, universal_newlines=True, timeout=30)
        except (OSError, subprocess.SubprocessError):
            return None
        return r.stdout.strip() if r.returncode == 0 else None
    top = git("rev-parse", "--show-toplevel")
    if not top or os.path.realpath(top) != os.path.realpath(root):
        return None
    head = git("rev-parse", "HEAD")
    if not head:
        return None
    kvers = sorted(d for d in os.listdir(root) if os.path.isdir(os.path.join(root, d, "sensor-src")))
    return {"commit": head,
            "dirty": bool(git("status", "--porcelain", "--untracked-files=no", "--", *kvers))}


def finding_key(fd):
    """What identifies a finding from one run to the next: no line number and
    no message, so that an unrelated edit of the file does not make it new."""
    return (fd.rel, fd.check, fd.mode, fd.severity)


def load_baseline(path):
    with open(path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    known = collections.Counter()
    for e in data.get("findings", []):
        known[(e["file"], e["check"], e["mode"], e["severity"])] += 1
    return known


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Cross-check every sensor driver's modes against itself "
                    "(init table, attr, set_fps arithmetic). Read-only.")
    ap.add_argument("--root", default=".", help="ingenic-sdk checkout (default: .)")
    ap.add_argument("--driver", action="append", default=[], metavar="GLOB",
                    help="only drivers matching this name or path glob, e.g. gc2053, "
                         "'sc2*', t31/gc2053, '4.4.94/*/t40/*' (repeatable)")
    ap.add_argument("--min-severity", choices=SEVERITIES, default="medium",
                    help="least severity to list (default: medium; the summary counts all)")
    ap.add_argument("--check", action="append", default=[], metavar="NAME", choices=sorted(CHECKS),
                    help="only these checks (repeatable; see --list-checks)")
    ap.add_argument("--format", choices=("text", "json"), default="text")
    ap.add_argument("--json", action="store_true", help="same as --format json")
    ap.add_argument("--modes", action="store_true",
                    help="text output: also list every resolved mode with its T / A / C")
    ap.add_argument("--notes", action="store_true",
                    help="text output: also list, per driver, what was left out of the analysis "
                         "(parameter combinations, #if conditions, jumps)")
    ap.add_argument("--unparsed", action="store_true",
                    help="text output: also list the drivers in which no mode was found "
                         "(drivers that cannot be parsed are always listed)")
    ap.add_argument("--follow-links", action="store_true",
                    help="also check the SoC folders that are symbolic links to another "
                         "folder (t10 -> t20, c100 -> t31): the same sources, read with "
                         "that SoC's own tx-isp-common.h")
    ap.add_argument("--baseline", metavar="FILE",
                    help="a file written by --write-baseline: list, and with --fail-on count, only "
                         "the findings that are not in it")
    ap.add_argument("--write-baseline", metavar="FILE",
                    help="record this run's findings of --min-severity and above in FILE")
    ap.add_argument("--fail-on", choices=SEVERITIES, default=None,
                    help="exit 1 when a finding of this severity or worse exists (whatever "
                         "--min-severity lists; with --baseline, a finding not in it), or when "
                         "a driver could not be analysed")
    ap.add_argument("--list-checks", action="store_true")
    opts = ap.parse_args(argv)
    if opts.json:
        opts.format = "json"

    if opts.list_checks:
        for name, (kind, text) in CHECKS.items():
            print("%-18s [%s]\n    %s" % (name, kind, text))
        return 0

    root = os.path.abspath(opts.root)
    drivers, aliases = find_drivers(root, opts.driver, opts.follow_links)
    if not drivers:
        sys.stderr.write("no sensor driver found under %s (run from the ingenic-sdk root, "
                         "or pass --root)\n" % root)
        return 2
    known = None
    if opts.baseline:
        try:
            known = load_baseline(opts.baseline)
        except (OSError, ValueError, KeyError, TypeError) as e:
            sys.stderr.write("cannot read baseline %s: %s\n" % (opts.baseline, e))
            return 2

    results = []
    for path, rel in drivers:
        try:
            results.append(analyze(root, path, rel))
        except Exception as e:      # a checker bug must not hide the other drivers
            r = DriverResult(rel)
            r.status = "error"
            r.reason = "checker error: %s: %s" % (type(e).__name__, e)
            results.append(r)

    rank = dict((s, i) for i, s in enumerate(SEVERITIES))
    limit = rank[opts.min_severity]
    counted = []
    for r in results:
        for fd in r.findings:
            if not opts.check or fd.check in opts.check:
                counted.append(fd)
    counted.sort(key=lambda fd: (rank[fd.severity], fd.check, fd.rel, fd.line))

    # with a baseline, a finding is new unless the baseline has it
    base_info = None
    fresh = counted
    if known is not None:
        left = collections.Counter(known)
        fresh = []
        for fd in counted:
            k = finding_key(fd)
            if left[k] > 0:
                left[k] -= 1
            else:
                fresh.append(fd)
        checked = set(r.rel for r in results)
        gone = sorted(k for k in left.elements() if k[0] in checked
                      and (not opts.check or k[1] in opts.check))
        base_info = {
            "file": opts.baseline,
            "known": len(counted) - len(fresh),
            "new": 0,       # of the severities listed; set below
            "gone": [{"file": k[0], "check": k[1], "mode": k[2], "severity": k[3]} for k in gone],
        }
    findings = [fd for fd in fresh if rank[fd.severity] <= limit]
    if base_info:
        base_info["new"] = len(findings)
    tree = tree_identity(root)

    if opts.write_baseline:
        rows = sorted(finding_key(fd) for fd in counted if rank[fd.severity] <= limit)
        with open(opts.write_baseline, "w", encoding="utf-8", newline="\n") as fh:
            json.dump({
                "version": VERSION,
                "tree": tree,
                "min_severity": opts.min_severity,
                "checks": sorted(opts.check) or None,
                "findings": [{"file": k[0], "check": k[1], "mode": k[2], "severity": k[3]} for k in rows],
            }, fh, indent=1)
            fh.write("\n")

    ok = [r for r in results if r.status == "ok"]
    errors = [r for r in results if r.status == "error"]
    left_out = collections.Counter()
    for r in results:
        left_out.update(r.left_out)
    summary = {
        "drivers": len(results),
        "unique_files": len(set(r.md5 for r in results)),
        "aliases_skipped": aliases,
        "analysed": len(ok),
        "no_modes": len([r for r in results if r.status == "no-modes"]),
        "errors": len(errors),
        "modes": sum(len(r.modes) for r in ok),
        "modes_with_table_frame": sum(1 for r in ok for m in r.modes if m.get("T") is not None),
        "modes_with_set_fps_frame": sum(1 for r in ok for m in r.modes if m.get("C") is not None),
        "combinations_left_out": dict((k, left_out[k]) for k in sorted(left_out)),
        "findings": {},
    }
    for fd in counted:
        d = summary["findings"].setdefault(fd.check, dict((s, 0) for s in SEVERITIES))
        d[fd.severity] += 1

    if opts.format == "json":
        json.dump({
            "version": VERSION,
            "tree": tree,
            "summary": summary,
            "baseline": base_info,
            "findings": [fd.as_dict() for fd in findings],
            "drivers": [{"file": r.rel, "status": r.status, "reason": r.reason,
                         "md5": r.md5, "notes": r.notes, "left_out": r.left_out,
                         "skipped_combinations": r.skipped, "modes": r.modes}
                        for r in results],
        }, sys.stdout, indent=1, default=str)
        sys.stdout.write("\n")
    else:
        for r in errors:
            print("%-6s %-17s %s  %s" % ("ERROR", "not-analysed", r.rel, r.reason))
        for fd in findings:
            d = fd.as_dict()
            print("%-6s %-17s %s/%s  %s:%d  [%s]  %s" % (
                fd.severity.upper(), fd.check, d["soc"], d["driver"], fd.rel, fd.line, fd.mode, fd.message))
        if opts.modes:
            print("")
            print("%-40s %-4s %-36s %-6s %6s %6s %6s %6s  %s" % (
                "file", "win", "mode", "fps", "T", "A", "C", "width", "note"))
            for r in ok:
                for m in r.modes:
                    note = ([m["T_reason"]] if m.get("T_reason") else []) + (m.get("notes") or [])
                    print("%-40s %-4s %-36s %-6s %6s %6s %6s %6s  %s" % (
                        r.rel, "[" + str(m.get("win")).split("[")[-1], m.get("mode"), fmt_fps(m.get("fps")),
                        m.get("T") if m.get("T") is not None else "-",
                        m.get("total_height") if m.get("total_height") is not None else "-",
                        m.get("C") if m.get("C") is not None else "-",
                        m.get("total_width") if m.get("total_width") is not None else "-",
                        "; ".join(note)))
        if opts.notes:
            print("")
            for r in results:
                for note in r.notes:
                    print("NOTE   %s: %s" % (r.rel, note))
        if opts.unparsed:
            print("")
            for r in results:
                if r.status == "no-modes":
                    print("%-9s %s: %s" % (r.status.upper(), r.rel, r.reason))
        print("")
        if tree:
            print("Tree:    %s%s" % (tree["commit"], " (with uncommitted driver changes)" if tree["dirty"] else ""))
        print("Drivers: %d files (%d distinct), %d analysed, %d without resolvable modes, %d not parsed"
              % (summary["drivers"], summary["unique_files"], summary["analysed"],
                 summary["no_modes"], summary["errors"]))
        if aliases:
            print("Aliases: %s (symbolic links, not checked a second time; --follow-links to include)"
                  % ", ".join(aliases))
        print("Modes:   %d resolved; init-table frame length read for %d, set_fps() frame length for %d"
              % (summary["modes"], summary["modes_with_table_frame"], summary["modes_with_set_fps_frame"]))
        if sum(left_out.values()):
            print("Left out: %d parameter combinations with a DVP init table and a MIPI interface "
                  "setting or the reverse, %d the driver refuses itself, %d the checker could not "
                  "follow (--notes lists them per driver)"
                  % (left_out["interface-table"], left_out["refused"],
                     left_out["goto"] + left_out["aborted"]))
        if base_info:
            print("Baseline: %s: %d findings known, %d new (listed above), %d of its entries no "
                  "longer found" % (base_info["file"], base_info["known"], base_info["new"],
                                    len(base_info["gone"])))
            for e in base_info["gone"][:20]:
                print("         gone: %s %s %s [%s]" % (e["severity"], e["check"], e["file"], e["mode"]))
            if len(base_info["gone"]) > 20:
                print("         ... and %d more (all of them in the JSON output)" % (len(base_info["gone"]) - 20))
        print("")
        print("%-18s %6s %6s %6s" % ("check", "high", "medium", "low"))
        for name in CHECKS:
            d = summary["findings"].get(name)
            if d:
                print("%-18s %6d %6d %6d" % (name, d["high"], d["medium"], d["low"]))
        tot = [sum(d[s] for d in summary["findings"].values()) for s in SEVERITIES]
        print("%-18s %6d %6d %6d" % ("total", tot[0], tot[1], tot[2]))

    if opts.fail_on is not None:
        # every finding counts, whatever --min-severity lists; and a driver
        # that could not be analysed is a failure, not a clean result
        if errors or any(rank[fd.severity] <= rank[opts.fail_on] for fd in fresh):
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
