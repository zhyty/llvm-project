"""
Test lldb-dap best-match breakpoints functionality
"""

from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
import os

import lldbdap_testcase
from dap_server import Source
import dap_server

dap_server.DEFAULT_TIMEOUT = 1000


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

        # Get the program path
        program = self.getBuildArtifact("a.out")

        # Build and launch program.
        # NOTE: need to set CWD to build directory so that the test fixture
        # main.cpp can find the dynamic lib.
        self.launch(program, cwd=self.getBuildDir(), preRunCommands=[f"settings set target.exec-search-paths {self.getBuildDir()}/lib"])

        # TODO: 

        # TODO: test relative path, absolute path

        # We're only setting one bp per src file.
        main_bp_id_1, main_bp_id_2, *_ = self.set_source_breakpoints(
            "main.cpp", [15, 22]
        )

        # TODO: why doesn't this get set in the right place?
        # utils_src = "best-match-breakpoints/lib/utils.cpp"
        # utils_src = self.get_src_full_path("lib/utils.cpp")
        # TODO(toyang): so it's properly setting up the utils.cpp filename-only breakpoint. But we're not seeing the breakpoint changed event... Why?

        lib_util_bp_id, *_ = self.set_source_breakpoints(
            "/some/fake/path/lib/utils.cpp",
            [7],
            wait_for_resolve=False,
        )

        # Continue execution - we should hit the first breakpoint in main.cpp
        self.dap_server.request_continue()
        self.verify_breakpoint_hit([main_bp_id_1])
        frames = self.dap_server.request_stackTrace()["body"]["stackFrames"]
        self.assertIn("main.cpp", frames[0]["source"]["name"])

        # Continue execution - we should hit the second breakpoint in main.cpp
        self.dap_server.request_continue()
        self.verify_breakpoint_hit([main_bp_id_2])
        frames = self.dap_server.request_stackTrace()["body"]["stackFrames"]
        self.assertIn("main.cpp", frames[0]["source"]["name"])

        # TODO(toyang): check if utils.cpp is a breakpoint, and how many locations
        self.dap_server.request_testGetTargetBreakpoints()

        # Continue to the next breakpoint - should hit lib/utils.cpp
        self.dap_server.request_continue()
        self.verify_breakpoint_hit([lib_util_bp_id])
        frames = self.dap_server.request_stackTrace()["body"]["stackFrames"]
        self.assertEqual(self.get_src_full_path("lib/utils.cpp"), frames[0]["source"]["path"])

        # # Verify the breakpoint_here variable is 42 (from lib/utils.cpp, not 99 from main/utils.cpp)
        # breakpoint_here = int(self.dap_server.get_local_variable_value("breakpoint_here"))
        # self.assertEqual(
        #     breakpoint_here,
        #     42,
        #     "Expected lib/utils.cpp breakpoint_here value (42), not main/utils.cpp value (99)",
        # )

    def get_src_full_path(self, src_filename: str) -> str:
        return os.path.join(self.getSourceDir(), src_filename)
