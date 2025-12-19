"""
Test lldb-dap best-match breakpoints functionality
"""

from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
import os

import lldbdap_testcase
from dap_server import Source
import dap_server


class TestDAP_bestMatchBreakpoints(lldbdap_testcase.DAPTestCaseBase):
    @skipIfWindows
    def test_best_match_breakpoints(self):
        """
        This test sets breakpoints at ambiguous file paths (main/main.cpp and
        lib/utils.cpp) using preRunCommands, and verifies that they are properly
        resolved and hit during execution. The test demonstrates best-match
        breakpoint functionality when there are multiple files with the same name
        in different directories.
        """
        self.build_and_create_debug_adapter(
            additional_args=["--use-best-match-breakpoints"]
        )

        program = self.getBuildArtifact("a.out")

        # The CWD is necessary for the debuggee to locate the helper .so.
        # The exec-search-path is necessary for the *debugger* to locate the
        # helper .so.
        self.launch(
            program,
            cwd=self.getBuildDir(),
            preRunCommands=[
                f"settings set target.exec-search-paths {self.getBuildDir()}/lib"
            ],
        )

        # We're only setting one bp per src file.
        main_bp_pre_load, main_bp_post_load, *_ = self.set_source_breakpoints(
            "main.cpp",
            [
                line_number("main.cpp", "// BREAK BEFORE .SO LOAD"),
                line_number("main.cpp", "// BREAK AFTER .SO LOAD"),
            ],
        )

        # Continue execution - we should hit the first breakpoint in main.cpp
        self.dap_server.request_continue()
        self.verify_breakpoint_hit([main_bp_pre_load])
        frames = self.dap_server.request_stackTrace()["body"]["stackFrames"]
        self.assertIn("main.cpp", frames[0]["source"]["name"])

        # This is what we're testing -- it should fall back to the "best match breakpoint, which is at first the root utils.cpp file.
        lib_util_bp_id, *_ = self.set_source_breakpoints(
            "/some/fake/path/lib/utils.cpp",
            [7],
        )
        current_bps = self.get_all_breakpoints()
        self.assertEqual(
            current_bps[int(lib_util_bp_id)]["source"]["path"],
            self.get_src_full_path("utils.cpp"),
        )

        # Continue execution - we should hit the second breakpoint in main.cpp
        self.dap_server.request_continue()
        self.verify_breakpoint_hit([main_bp_post_load])
        frames = self.dap_server.request_stackTrace()["body"]["stackFrames"]
        self.assertIn("main.cpp", frames[0]["source"]["name"])

        # After the .so load, we should have a new breakpoint at lib/utils.cpp
        current_bps = self.get_all_breakpoints()
        self.assertEqual(
            current_bps[int(lib_util_bp_id)]["source"]["path"],
            self.get_src_full_path("lib/utils.cpp"),
        )

        # Continue to the next breakpoint - should hit lib/utils.cpp
        self.dap_server.request_continue()
        self.verify_breakpoint_hit([lib_util_bp_id])
        frames = self.dap_server.request_stackTrace()["body"]["stackFrames"]
        self.assertEqual(
            self.get_src_full_path("lib/utils.cpp"), frames[0]["source"]["path"]
        )

    def get_src_full_path(self, src_filename: str) -> str:
        return os.path.join(self.getSourceDir(), src_filename)
