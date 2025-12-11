import os
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *


class TestDyldModuleLoad(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @skipUnlessPlatform(["linux"])
    def test_dyld_load(self):
        self.build()
        exe = self.getBuildArtifact("a.out")

        target = self.dbg.CreateTarget(exe)
        self.assertTrue(target and target.IsValid(), "Target is valid")

        # TODO(toyang): this fixes it. It's probably an issue with finding the dylib.
        # Pre-register the shared library with the target so LLDB knows about it.
        # The library is built in lib/ subdirectory, so we need to provide the full path.
        # lib_path = self.getBuildArtifact("lib/libhelper.so")
        # self.assertTrue(os.path.exists(lib_path), f"Library not found at {lib_path}")
        # target.AddModule(lib_path, None, None, None)

        self.runCmd(f"settings append target.exec-search-paths {self.getBuildDir()}/lib")

        # Pre libhelper 
        breakpoint = target.BreakpointCreateByLocation("main.cpp", 15)

        # Post libhelper
        # This should create a breakpoint in the stepping thread.
        breakpoint = target.BreakpointCreateByLocation("main.cpp", 22)
        self.assertTrue(breakpoint and breakpoint.IsValid(), "Breakpoint is valid")

        self.runCmd("log enable lldb dyld --verbose")

        # Run the program.
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        print(f"process_working_directory: {self.get_process_working_directory()}")
        self.assertTrue(process and process.IsValid(), PROCESS_IS_VALID)

        # The stop reason of the thread should be breakpoint.
        self.assertEqual(process.GetState(), lldb.eStateStopped, PROCESS_STOPPED)

        # Go to second breakpoint
        process.Continue()

        # The stop reason of the thread should be breakpoint.
        self.assertEqual(process.GetState(), lldb.eStateStopped, PROCESS_STOPPED)

        self.assertTrue(
            any(
                module.GetFileSpec().GetFilename() == "libhelper.so"
                for module in target.modules
            ),
            f"libhelper.so is not found in {target.modules}",
        )

        utils_breakpoint = target.BreakpointCreateByLocation("utils.cpp", 7)
        self.assertEqual(utils_breakpoint.GetNumLocations(), 2)
