#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""
Tests for tools/check_sensor_modes.py. Standard library only:

    python3 tools/tests/test_check_sensor_modes.py

The fixture tree under sensor-modes/ has the per-kernel layout and is
never built. It holds frozen copies of four drivers, so that a later fix of
the real driver does not change what is tested here:

  4.4.94/sensor-src/t31/gc2053.c   as of 01fc6853, before pull request 13:
                                   the 30 fps MIPI table programs 1181 lines,
                                   the mode reports 1418
  4.4.94/sensor-src/t32/gc13b0.c   as of 1ecb6041: frame-length bytes in the
                                   opposite order in the table and set_fps()
  3.10.14/sensor-src/t20/sp1409.c  as of 1ecb6041: set_fps() reads registers
                                   on a page the init table does not write
  3.10.14/sensor-src/t31/jxf32.c   as of 1ecb6041: a mode that takes the
                                   attributes of another window

with each SoC's tx-isp-common.h (the only header copied), and four small
files written for the tests (fx_*.c; each says in its header what it is
for).

expected.json is the script's whole JSON output for that tree. The merged
`common/sensor/<soc>` layout is built in a temporary directory from the
same fixtures. After a change that is meant to alter the output, rewrite
expected.json and read the diff:

    UPDATE_EXPECTED=1 python3 tools/tests/test_check_sensor_modes.py
"""

import contextlib
import io
import json
import os
import shutil
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))
sys.dont_write_bytecode = True      # no __pycache__ beside the script

import check_sensor_modes as csm      # noqa: E402

TREE = os.path.join(HERE, "sensor-modes")
EXPECTED = os.path.join(TREE, "expected.json")

GC2053 = "4.4.94/sensor-src/t31/gc2053.c"
GC13B0 = "4.4.94/sensor-src/t32/gc13b0.c"
SP1409 = "3.10.14/sensor-src/t20/sp1409.c"
JXF32 = "3.10.14/sensor-src/t31/jxf32.c"
FX = "4.4.94/sensor-src/t31/fx_%s.c"


def run_root(root, *args):
    """Run the script on a tree: (exit code, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        try:
            code = csm.main(["--root", root] + list(args))
        except SystemExit as e:     # argparse
            code = e.code
    return code, out.getvalue(), err.getvalue()


def run(*args):
    """Run the script on the fixture tree: (exit code, stdout, stderr)."""
    return run_root(TREE, *args)


def run_json(*args):
    code, out, err = run("--json", *args)
    return code, json.loads(out)


class FixtureTree(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.code, cls.data = run_json("--min-severity", "low")
        cls.drivers = dict((d["file"], d) for d in cls.data["drivers"])

    def modes(self, rel):
        return dict((m["mode"], m) for m in self.drivers[rel]["modes"])

    def findings(self, rel, check=None):
        return [f for f in self.data["findings"]
                if f["file"] == rel and (check is None or f["check"] == check)]

    def test_runs_clean_without_fail_on(self):
        self.assertEqual(self.code, 0)
        self.assertEqual(self.data["version"], csm.VERSION)

    def test_expected_output(self):
        got = dict(self.data)
        got.pop("tree")             # the commit of whatever checkout holds the fixtures
        text = json.dumps(got, indent=1, sort_keys=True, default=str) + "\n"
        if os.environ.get("UPDATE_EXPECTED"):
            with open(EXPECTED, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(text)
        with open(EXPECTED, "r", encoding="utf-8") as fh:
            want = fh.read()
        self.assertEqual(text, want, "output differs from expected.json "
                         "(UPDATE_EXPECTED=1 rewrites it; read the diff)")

    def test_gc2053_table_against_total_height(self):
        m = self.modes(GC2053)["sensor_max_fps=30,data_interface=MIPI"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1181, 1418, 1181))
        f = self.findings(GC2053, "frame-length")
        self.assertEqual([(x["severity"], x["mode"]) for x in f],
                         [("high", "sensor_max_fps=30,data_interface=MIPI")])
        # the modes that agree stay quiet
        m = self.modes(GC2053)["sensor_max_fps=15,data_interface=MIPI"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1181, 1181, 1181))

    def test_gc13b0_byte_order_reports_the_table_frame(self):
        f = self.findings(GC13B0, "byte-order")
        self.assertEqual([x["severity"] for x in f], ["high"])
        m = self.modes(GC13B0)["-"]
        # T is the frame the table holds, not the table's bytes decoded in
        # set_fps()'s order, and it agrees with the finding's own text
        self.assertEqual(m["T"], 1696)
        self.assertEqual(m["T_as_set_fps_reads"], 40966)
        self.assertIn("1696 lines read the table's way", f[0]["message"])

    def test_sp1409_page_register_read(self):
        # set_fps() reads the page register, finds page 1 and reads hts
        # there; the init table writes hts on page 0 only, so the value is
        # not known. It used to be taken from page 0 (hts 2).
        m = self.modes(SP1409)["-"]
        self.assertIsNone(m["C"])
        self.assertIsNone(m["hts"])
        self.assertEqual(self.findings(SP1409), [])

    def test_jxf32_mode_with_another_windows_attributes(self):
        m = self.modes(JXF32)["sensor_resolution=RES_30"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1133, 400, 1200))
        checks = sorted(set(f["check"] for f in self.findings(JXF32)
                            if f["mode"] == "sensor_resolution=RES_30" and f["severity"] != "low"))
        self.assertEqual(checks, ["frame-length", "frame-short", "image-size"])

    def test_goto_is_followed(self):
        d = self.drivers[FX % "goto"]
        self.assertEqual(d["status"], "ok")
        m = self.modes(FX % "goto")["-"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1200, 1500, 1200))
        self.assertEqual([f["severity"] for f in self.findings(FX % "goto", "frame-length")], ["high"])
        self.assertEqual(d["left_out"]["goto"], 0)

    def test_default_branch(self):
        d = self.drivers[FX % "default"]
        got = sorted((m["mode"], m["T"], m["total_height"], m["C"]) for m in d["modes"])
        self.assertEqual(got, [
            ("sensor_max_fps=25,data_interface=MIPI", 2400, 2400, 2400),   # the default: branch
            ("sensor_max_fps=30,data_interface=MIPI", 1200, 1200, 1200),
        ])
        # DVP reaches a default: that only logs: refused, and said so
        self.assertEqual(d["left_out"]["refused"], 2)
        self.assertTrue(any("refuses" in n for n in d["notes"]))
        self.assertEqual(self.findings(FX % "default"), [])

    def test_preprocessor_conditions(self):
        d = self.drivers[FX % "ppif"]
        m = self.modes(FX % "ppif")["-"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1200, 1200, 1200))
        self.assertEqual(m["max_integration_time"], 1196)
        self.assertTrue(any("FX_VERSION" in n and "not read" in n for n in d["notes"]), d["notes"])
        self.assertEqual([(f["severity"], f["check"]) for f in self.findings(FX % "ppif")],
                         [("low", "table-name")])

    def test_file_that_does_not_parse(self):
        d = self.drivers[FX % "broken"]
        self.assertEqual(d["status"], "error")
        self.assertIn("line 21", d["reason"])
        self.assertEqual(self.data["summary"]["errors"], 1)

    def test_left_out_combinations_are_listed(self):
        d = self.drivers[GC13B0]
        self.assertEqual(d["left_out"]["interface-table"], len(d["skipped_combinations"]))
        self.assertGreater(len(d["skipped_combinations"]), 0)
        self.assertEqual(self.data["summary"]["combinations_left_out"]["interface-table"],
                         sum(x["left_out"].get("interface-table", 0) for x in self.data["drivers"]))


class CommonLayout(unittest.TestCase):
    @staticmethod
    def copy_fixtures(root):
        """The 4.4.94 fixture drivers under common/sensor and common/isp."""
        kver = "4.4.94"
        for soc in ("t31", "t32"):
            src = os.path.join(TREE, kver, "sensor-src", soc)
            dst = os.path.join(root, "common", "sensor", soc)
            os.makedirs(dst, exist_ok=True)
            for fn in os.listdir(src):
                if fn.endswith(".c"):
                    shutil.copy2(os.path.join(src, fn), os.path.join(dst, fn))
            src = os.path.join(TREE, kver, "isp", soc, "include")
            dst = os.path.join(root, "common", "isp", soc, "include")
            os.makedirs(dst, exist_ok=True)
            for fn in os.listdir(src):
                shutil.copy2(os.path.join(src, fn), os.path.join(dst, fn))

    def test_common_sensor_tree(self):
        with tempfile.TemporaryDirectory() as tmp:
            self.copy_fixtures(tmp)
            code, out, err = run_root(tmp, "--json", "--driver", "gc2053",
                                      "--check", "frame-length")
            self.assertEqual(code, 0, err)
            data = json.loads(out)
        drivers = dict((d["file"], d) for d in data["drivers"])
        rel = "common/sensor/t31/gc2053.c"
        self.assertIn(rel, drivers)
        modes = dict((m["mode"], m) for m in drivers[rel]["modes"])
        m = modes["sensor_max_fps=30,data_interface=MIPI"]
        self.assertEqual((m["T"], m["total_height"], m["C"]), (1181, 1418, 1181))
        self.assertEqual([(f["file"], f["check"], f["severity"]) for f in data["findings"]],
                         [(rel, "frame-length", "high")])

    def test_t31_pp_headers_on_3_10_14(self):
        with tempfile.TemporaryDirectory() as tmp:
            for sub, value in (("t31-pp", "1"), ("t31", "2")):
                d = os.path.join(tmp, "common", "isp", sub, "include")
                os.makedirs(d)
                with open(os.path.join(d, "layout.h"), "w", encoding="ascii") as fh:
                    fh.write("#define LAYOUT_TEST %s\n" % value)
            old = csm.header_consts(tmp, "common", "", "t31", "3.10.14")
            new = csm.header_consts(tmp, "common", "", "t31", "4.4.94")
        self.assertEqual(old["LAYOUT_TEST"], 1)
        self.assertEqual(new["LAYOUT_TEST"], 2)


class CommandLine(unittest.TestCase):
    def test_fail_on_counts_what_min_severity_hides(self):
        # fx_ppif has one finding, a low one; the default listing starts at medium
        code, out, _ = run("--driver", "fx_ppif", "--fail-on", "low")
        self.assertEqual(code, 1)
        self.assertNotIn("LOW ", out)
        self.assertEqual(run("--driver", "fx_ppif", "--fail-on", "medium")[0], 0)
        self.assertEqual(run("--driver", "fx_ppif")[0], 0)

    def test_fail_on_fails_on_a_file_that_does_not_parse(self):
        code, out, _ = run("--driver", "fx_broken", "--fail-on", "high")
        self.assertEqual(code, 1)
        self.assertIn("ERROR  not-analysed", out)
        code, out, _ = run("--driver", "fx_broken")
        self.assertEqual(code, 0)
        self.assertIn("ERROR  not-analysed", out)

    def test_unknown_check_name_is_refused(self):
        code, _, err = run("--driver", "gc2053", "--check", "byteorder")
        self.assertEqual(code, 2)
        self.assertIn("byteorder", err)
        self.assertEqual(run("--driver", "gc2053", "--check", "byte-order")[0], 0)

    def test_no_driver_matches(self):
        self.assertEqual(run("--driver", "no_such_sensor")[0], 2)

    def test_modes_table_has_a_header_for_every_column(self):
        _, out, _ = run("--driver", "gc13b0", "--modes")
        lines = out.splitlines()
        head = next(l for l in lines if l.startswith("file "))
        self.assertEqual(head.split(), ["file", "win", "mode", "fps", "T", "A", "C", "width", "note"])
        row = next(l for l in lines if l.startswith(GC13B0))
        self.assertEqual(row.split()[4:7], ["1696", "1696", "1696"])
        self.assertIn("40966", row)

    def test_notes(self):
        _, out, _ = run("--driver", "fx_default", "--notes")
        self.assertIn("NOTE   " + FX % "default", out)
        self.assertNotIn("NOTE", run("--driver", "fx_default")[1])

    def test_baseline(self):
        sel = ["--driver", "gc2053", "--driver", "gc13b0", "--driver", "jxf32"]
        with tempfile.TemporaryDirectory() as tmp:
            path = os.path.join(tmp, "baseline.json")
            code, _, _ = run("--write-baseline", path, *sel)
            self.assertEqual(code, 0)
            with open(path, "r", encoding="utf-8") as fh:
                base = json.load(fh)
            self.assertEqual(base["min_severity"], "medium")
            self.assertEqual(len(base["findings"]), 7)

            # everything known: nothing listed, nothing to fail on
            code, data = run_json("--baseline", path, "--fail-on", "medium", *sel)
            self.assertEqual(code, 0)
            self.assertEqual(data["findings"], [])
            self.assertEqual((data["baseline"]["known"], data["baseline"]["new"]), (7, 0))
            self.assertEqual(data["baseline"]["gone"], [])
            # the summary still counts every finding
            self.assertEqual(sum(v["high"] for v in data["summary"]["findings"].values()), 5)

            # a baseline of medium and above does not cover the low findings
            self.assertEqual(run("--baseline", path, "--fail-on", "low", *sel)[0], 1)

            # one entry less: that finding is new, and fails the run
            dropped = base["findings"].pop(0)
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(base, fh)
            code, data = run_json("--baseline", path, "--fail-on", "medium", *sel)
            self.assertEqual(code, 1)
            self.assertEqual([(f["file"], f["check"], f["mode"]) for f in data["findings"]],
                             [(dropped["file"], dropped["check"], dropped["mode"])])

            # one entry too many: reported as gone, and does not fail
            base["findings"].append(dropped)
            base["findings"].append({"file": GC2053, "check": "frame-short",
                                     "mode": "sensor_max_fps=15,data_interface=MIPI", "severity": "high"})
            with open(path, "w", encoding="utf-8") as fh:
                json.dump(base, fh)
            code, data = run_json("--baseline", path, "--fail-on", "medium", *sel)
            self.assertEqual(code, 0)
            self.assertEqual([g["check"] for g in data["baseline"]["gone"]], ["frame-short"])

            # a file that does not parse fails the run whatever the baseline says
            self.assertEqual(run("--baseline", path, "--fail-on", "high")[0], 1)

    def test_unreadable_baseline(self):
        code, _, err = run("--driver", "gc2053", "--baseline", os.path.join(TREE, "no-such-file.json"))
        self.assertEqual(code, 2)
        self.assertIn("cannot read baseline", err)


class PreprocessorExpressions(unittest.TestCase):
    DEFINES = {"FLAGS": ("3", 1), "NEXT": ("(FLAGS + 1)", 2), "EMPTY": ("", 3),
               "LOOP_A": ("LOOP_B", 4), "LOOP_B": ("LOOP_A", 5)}

    def cond(self, text):
        return csm.pp_condition("if", text, self.DEFINES)

    def test_true(self):
        for text in ("1", "100/5", "1 & 1", "1<<3", "(FLAGS & 1)", "NEXT == 4", "0x10 == 16",
                     "10UL > 9", "~0 == -1", "!0", "-7/2 == -3", "-7%2 == -1", "1 ? 2 : 0",
                     "defined(FLAGS) && !defined(NOPE)", "defined FLAGS", "FLAGS ^ 1", "0 || FLAGS | 4",
                     "(1 << 2) + 2 * 3 == 10", "FLAGS >= 3 && FLAGS <= 3 && FLAGS != 2"):
            self.assertTrue(self.cond(text), text)

    def test_false(self):
        for text in ("0", "FLAGS & 4", "NOT_A_MACRO", "defined(NOPE)", "1 ? 0 : 2", "FLAGS < 3", "2 - 2"):
            self.assertFalse(self.cond(text), text)

    def test_not_read_is_false_and_reported(self):
        for text in ("9**9**9", "1/0", "1 % 0", "1 << 64", "1 << -1", "VERSION(4,4,0) > 1", "EMPTY",
                     "LOOP_A", "1 +", "(1", "1 2", "__import__('os').system('true')", "'a' == 97"):
            unread = []
            self.assertFalse(csm.pp_condition("if", text, self.DEFINES, unread, 7), text)
            self.assertEqual([(u[0], u[1]) for u in unread], [(7, text)], text)

    def test_ifdef(self):
        self.assertTrue(csm.pp_condition("ifdef", "FLAGS", self.DEFINES))
        self.assertFalse(csm.pp_condition("ifdef", "NOPE", self.DEFINES))
        self.assertTrue(csm.pp_condition("ifndef", "NOPE", self.DEFINES))

    def test_dropped_block_keeps_line_numbers(self):
        text, _ = csm.preprocess("a\n#if (3 & 1)\nb\n#else\nc\n#endif\nd", {})
        self.assertEqual(text.split("\n"), ["a", "", "b", "", "", "", "d"])


class Interpreter(unittest.TestCase):
    """The statement forms the review of the first version found wrong."""

    def run_c(self, body):
        text = "static int f(void)\n{\n%s\n}\n" % body
        unit = csm.Unit("mem.c", "4.4.94/sensor-src/t31/mem.c", text, {})
        it = csm.Interp(unit, {}, None)
        return it.run_func("f", []), it

    def test_goto_forward_to_a_label_of_the_function(self):
        ret, it = self.run_c("int r = 1;\ngoto out;\nr = 2;\nout:\nreturn r;")
        self.assertEqual(ret, 1)
        self.assertEqual(it.gotos, [])

    def test_goto_out_of_nested_blocks(self):
        ret, _ = self.run_c("int r = 0;\nint i;\nfor (i = 0; i < 4; i++) {\n"
                            "if (i == 2) {\nr = 7;\ngoto done;\n}\n}\nr = 9;\ndone:\nreturn r;")
        self.assertEqual(ret, 7)

    def test_goto_backward_ends(self):
        ret, _ = self.run_c("int n = 0;\nagain:\nn = n + 1;\nif (n < 3)\ngoto again;\nreturn n;")
        self.assertEqual(ret, 3)

    def test_goto_backward_without_end_is_stopped(self):
        with self.assertRaises(csm.Abort):
            self.run_c("again:\ngoto again;\nreturn 0;")

    def test_goto_into_a_block_is_reported_not_followed(self):
        ret, it = self.run_c("int r = 1;\ngoto in;\nif (r) {\nin:\nr = 2;\n}\nreturn r;")
        self.assertEqual([(g[0], g[1]) for g in it.gotos], [("f", "in")])

    def test_switch_default_that_does_work_is_taken(self):
        ret, it = self.run_c("int r = 0;\nswitch (5) {\ncase 1:\nr = 1;\nbreak;\ndefault:\nr = 2;\nbreak;\n}\nreturn r;")
        self.assertEqual(ret, 2)
        self.assertEqual(it.nomatch, [])

    def test_switch_default_that_only_logs_is_a_refusal(self):
        ret, it = self.run_c("int r = 0;\nswitch (5) {\ncase 1:\nr = 1;\nbreak;\ndefault:\n"
                             "ISP_ERROR(\"no\\n\");\n}\nreturn r;")
        self.assertEqual(len(it.nomatch), 1)

    def test_switch_without_default(self):
        ret, it = self.run_c("int r = 0;\nswitch (5) {\ncase 1:\nr = 1;\nbreak;\n}\nreturn r;")
        self.assertEqual((ret, len(it.nomatch)), (0, 1))


if __name__ == "__main__":
    unittest.main()
