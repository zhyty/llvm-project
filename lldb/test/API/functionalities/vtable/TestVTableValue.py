"""
Make sure the getting a variable path works and doesn't crash.
"""


import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *


class TestVTableValue(TestBase):
    # If your test case doesn't stress debug info, then
    # set this to true.  That way it won't be run once for
    # each debug info format.
    NO_DEBUG_INFO_TESTCASE = True

    def test_vtable(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "At the end", lldb.SBFileSpec("main.cpp")
        )

        rect = self.frame().FindVariable("rect")
        vtable = rect.GetVTable()
        self.assertEquals(vtable.GetName(), "vtable for Rectangle")
        self.assertEquals(vtable.GetTypeName(), "vtable for Rectangle")

        # Count both destructors
        self.assertEquals(vtable.GetNumChildren(), 4)

        # Verify vtable address
        vtable_addr = int(vtable.GetLocation(), 16)
        self.assertEquals(vtable_addr, self.expected_vtable_addr(rect))

        for (idx, vtable_entry) in enumerate(vtable.children):
            self.verify_vtable_entry(vtable_entry, vtable_addr, idx)

    def test_base_class_ptr(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "Shape is Rectangle", lldb.SBFileSpec("main.cpp")
        )

        shape_ptr = self.frame().FindVariable("shape_ptr")
        self.assertEquals(shape_ptr.GetVTable().GetName(), "vtable for Rectangle")

        # TODO: the vtable SBValue doesn't correctly update here
        lldbutil.run_to_source_breakpoint(
            self, "Shape is Shape", lldb.SBFileSpec("main.cpp")
        )
        self.assertEquals(shape_ptr.GetVTable().GetName(), "vtable for Shape")

    def test_no_vtable(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "At the end", lldb.SBFileSpec("main.cpp")
        )

        not_subclass = self.frame().FindVariable("not_subclass")
        vtable = not_subclass.GetVTable()
        # TODO: we need additional logic in `SBValue::GetVTable`. It seems like `UpdateValue` isn't called until after creation.
        # self.assertFalse(vtable.IsValid())

    def expected_vtable_addr(self, var: lldb.SBValue) -> int:
        load_addr = var.GetLoadAddress()
        read_from_memory_error = lldb.SBError()
        vtable_addr = self.process().ReadPointerFromMemory(
            load_addr, read_from_memory_error
        )
        self.assertTrue(read_from_memory_error.Success())
        return vtable_addr

    def expected_vtable_entry_func_ptr(self, vtable_addr: int, vtable_entry_idx: int):
        vtable_entry_addr = (
            vtable_addr + vtable_entry_idx * self.process().GetAddressByteSize()
        )
        read_func_ptr_error = lldb.SBError()
        func_ptr = self.process().ReadPointerFromMemory(
            vtable_entry_addr, read_func_ptr_error
        )
        self.assertTrue(read_func_ptr_error.Success())
        return func_ptr

    def verify_vtable_entry(
        self, vtable_entry: lldb.SBValue, vtable_addr: int, vtable_entry_idx: int
    ):
        """Verify the vtable entry looks something like:

        (double ()) [0] = 0x0000000100003a10 a.out`Rectangle::Area() at main.cpp:14

        """
        # Check function ptr
        vtable_entry_func_ptr = int(vtable_entry.GetValue(), 16)
        self.assertEquals(
            vtable_entry_func_ptr,
            self.expected_vtable_entry_func_ptr(vtable_addr, vtable_entry_idx),
        )

        sb_addr = self.target().ResolveLoadAddress(vtable_entry_func_ptr)
        sym_ctx = sb_addr.GetSymbolContext(lldb.eSymbolContextEverything)

        self.assertEquals(vtable_entry.GetType(), sym_ctx.GetFunction().GetType())

        self.assertIn(
            sym_ctx.GetFunction().GetDisplayName(),
            vtable_entry.GetSummary(),
        )
        self.assertIn(
            sym_ctx.GetLineEntry().GetFileSpec().GetFilename(),
            vtable_entry.GetSummary(),
        )
        self.assertIn(str(sym_ctx.GetLineEntry().GetLine()), vtable_entry.GetSummary())
