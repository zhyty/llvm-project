// TODO(toyang): make sure this name is consistent
//===-- ValueObjectVTable.cpp ---------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "lldb/Core/ValueObjectVTable.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ValueObjectChild.h"
#include "lldb/Symbol/Function.h"
#include "lldb/lldb-defines.h"
#include "lldb/lldb-enumerations.h"
#include "lldb/lldb-forward.h"
#include "lldb/lldb-private-enumerations.h"
#include "clang/Tooling/Transformer/RangeSelector.h"
#include "llvm/Support/MathExtras.h"
#include <cstdint>

using namespace lldb;
using namespace lldb_private;

class ValueObjectVTableChild : public ValueObject {
public:
  ValueObjectVTableChild(ValueObject &parent, uint32_t func_idx,
                         uint64_t addr_size)
      : ValueObject(parent), m_func_idx(func_idx), m_addr_size(addr_size) {
    // TODO: I don't think this works
    SetFormat(lldb::eFormatHex);
    SetName(ConstString(llvm::formatv("[{0}]", func_idx).str()));
  }

  ~ValueObjectVTableChild() override = default;

  std::optional<uint64_t> GetByteSize() override { return m_addr_size; };

  size_t CalculateNumChildren(uint32_t max) override { return 0; };

  ValueType GetValueType() const override { return eValueTypeVTableEntry; };

  bool IsInScope() override {
    ValueObject *parent = GetParent();
    if (!parent)
      return false;
    return parent->IsInScope();
  };

  // TODO: just a note, but I don't see many examples of this being overridden
  // in the codebase. I ended up choosing to override it here because it was
  // difficult working with the default formatting logic. From what I could
  // understand, the default formatting logic expects Scalars to be backed by
  // some valid compiler type. See `TypeFormatImpl_Format` for more details, and
  // I think the data extractor stuff might be relevant. I've tried explicitly
  // setting the compiler type with
  // `m_value.SetCompilerType(GetCompilerType())`, but that also didn't seem to
  // work.
  bool GetValueAsCString(const lldb_private::TypeFormatImpl &format,
                         std::string &destination) override {
    if (UpdateValueIfNeeded(false)) {
      addr_t vtable_entry_addr = GetValue().GetScalar().ULongLong(LLDB_INVALID_ADDRESS);
      StreamString strm;
      strm.Printf("0x%16.16" PRIx64, vtable_entry_addr);
      destination = strm.GetString();
      return true;
    } else {
      return false;
    }
  }

protected:
  bool UpdateValue() override {
    SetValueIsValid(false);
    m_value.Clear();
    ValueObject *parent = GetParent();
    if (!parent) {
      m_error.SetErrorString("no parent object");
      return false;
    }

    addr_t parent_addr =
        parent->GetValue().GetScalar().ULongLong(LLDB_INVALID_ADDRESS);
    if (parent_addr == LLDB_INVALID_ADDRESS) {
      m_error.SetErrorString("parent has invalid address");
      return false;
    }

    ProcessSP process_sp = GetProcessSP();
    if (!process_sp) {
      m_error.SetErrorString("no process");
      return false;
    }

    TargetSP target_sp = GetTargetSP();
    if (!target_sp) {
      m_error.SetErrorString("no target");
      return false;
    }

    // Each `vtable_entry_addr` points to the function pointer.
    addr_t vtable_entry_addr = parent_addr + m_func_idx * m_addr_size;
    addr_t vfunc_ptr =
        process_sp->ReadPointerFromMemory(vtable_entry_addr, m_error);
    if (m_error.Fail()) {
      m_error.SetErrorStringWithFormat(
          "failed to read virtual function entry 0x%16.16" PRIx64,
          vtable_entry_addr);
      return false;
    }

    Address resolved_vfunc_ptr_address;
    target_sp->ResolveLoadAddress(vfunc_ptr, resolved_vfunc_ptr_address);
    if (!resolved_vfunc_ptr_address.IsValid()) {
      m_error.SetErrorStringWithFormat(
          "unable to resolve func ptr address: 0x%16.16" PRIx64, vfunc_ptr);
      return false;
    }

    // Update ValueObject state

    resolved_vfunc_ptr_address.CalculateSymbolContext(&m_sym_ctx);

    // NOTE: when this is a scalar, `ValueObject::GetPointerValue` treats this
    // value as the actual pointer. When this is a `LoadAddress`, it goes
    // through `m_data`. This is important when working with
    // `CXXFunctionPointerSummaryProvider`, which is the formatter that ends up
    // being set for this object during the
    // `ValueObject::UpdateFormatsIfNeeded()` call.
    m_value.SetValueType(Value::ValueType::Scalar);
    m_value.GetScalar() = vfunc_ptr;

    SetValueDidChange(true);
    SetValueIsValid(true);
    return true;
  };

  CompilerType GetCompilerTypeImpl() override {
    if (m_sym_ctx.function)
      return m_sym_ctx.function->GetCompilerType();
    return CompilerType();
  };

  const uint32_t m_func_idx;
  const uint64_t m_addr_size;
  SymbolContext m_sym_ctx;

private:
  // For ValueObject only
  ValueObjectVTableChild(const ValueObjectVTableChild &) = delete;
  const ValueObjectVTableChild &
  operator=(const ValueObjectVTableChild &) = delete;
};

ValueObjectSP ValueObjectVTable::Create(ValueObject &parent) {
  return (new ValueObjectVTable(parent))->GetSP();
}

ValueObjectVTable::ValueObjectVTable(ValueObject &parent)
    : ValueObject(parent) {}

std::optional<uint64_t> ValueObjectVTable::GetByteSize() {
  if (m_vtable_symbol)
    return m_vtable_symbol->GetByteSize();
  else
    return std::nullopt;
}

size_t ValueObjectVTable::CalculateNumChildren(uint32_t max) {
  return m_num_vtable_entries <= max ? m_num_vtable_entries : max;
}

ValueType ValueObjectVTable::GetValueType() const { return eValueTypeVTable; }

ConstString ValueObjectVTable::GetTypeName() {
  if (m_vtable_symbol)
    return m_vtable_symbol->GetName();
  return ConstString();
}

ConstString ValueObjectVTable::GetQualifiedTypeName() { return GetTypeName(); }

ConstString ValueObjectVTable::GetDisplayTypeName() {
  if (m_vtable_symbol)
    return m_vtable_symbol->GetDisplayName();
  return ConstString();
}

bool ValueObjectVTable::IsInScope() { return GetParent()->IsInScope(); }

ValueObject *ValueObjectVTable::CreateChildAtIndex(size_t idx,
                                                   bool synthetic_array_member,
                                                   int32_t synthetic_index) {
  if (synthetic_array_member)
    return nullptr;
  return new ValueObjectVTableChild(*this, idx, m_addr_size);
}

bool ValueObjectVTable::UpdateValue() {
  m_error.Clear();
  SetValueIsValid(false);
  ValueObject *parent = GetParent();
  if (!parent) {
    m_error.SetErrorString("no parent object");
    return false;
  }

  if (!parent->UpdateValueIfNeeded(false)) {
    m_error.SetErrorString("failed to update parent");
    return false;
  }

  // TODO(toyang): check parent's GetCompilerType and see if it has virtual
  // functions
  TargetSP target_sp = GetTargetSP();
  if (!target_sp) {
    m_error.SetErrorString("no target");
    return false;
  }

  // TODO(toyang): put this into valueobject, refactor SBValue::GetLoadAddress()
  AddressType addr_type;
  addr_t parent_load_addr =
      parent->GetAddressOf(/*scalar_is_load_address=*/true, &addr_type);
  if (addr_type == eAddressTypeFile) {
    ModuleSP module_sp(parent->GetModule());
    if (!module_sp) {
      parent_load_addr = LLDB_INVALID_ADDRESS;
    } else {
      Address addr;
      module_sp->ResolveFileAddress(parent_load_addr, addr);
      parent_load_addr = addr.GetLoadAddress(target_sp.get());
    }
  } else if (addr_type == eAddressTypeHost ||
             addr_type == eAddressTypeInvalid) {
    parent_load_addr = LLDB_INVALID_ADDRESS;
  }

  if (parent_load_addr == LLDB_INVALID_ADDRESS) {
    m_error.SetErrorString("parent is not in memory");
    return false;
  }

  // TODO: if process_sp->ReadPointerFromMemory(parent_load_addr, ...) is the
  // same VTable address as what we previously checked (need to store this),
  // then we don't need to do anything.

  m_value.Clear();

  ProcessSP process_sp = GetProcessSP();
  if (!process_sp) {
    m_error.SetErrorString("no process");
    return false;
  }

  // We expect to find the vtable at the first block of memory.
  addr_t possible_vtable_ptr =
      process_sp->ReadPointerFromMemory(parent_load_addr, m_error);
  if (m_error.Fail())
    return false;

  Address resolved_possible_vtable_address;
  target_sp->ResolveLoadAddress(possible_vtable_ptr,
                                resolved_possible_vtable_address);
  if (!resolved_possible_vtable_address.IsValid()) {
    m_error.SetErrorStringWithFormat(
        "Unable to resolve address 0x%16.16" PRIx64, possible_vtable_ptr);
    return false;
  }

  m_vtable_symbol =
      resolved_possible_vtable_address.CalculateSymbolContextSymbol();
  if (!m_vtable_symbol) {
    m_error.SetErrorString("not a vtable");
    return false;
  }
  // TODO(toyang): If symbol types (lldb-enumerations) contain VTable types,
  // then we could verify it here. Dunno if we can though.
  llvm::StringRef symbol_name = m_vtable_symbol->GetName().GetStringRef();
  // TODO: deal with the case where the first field of the struct is an object
  // with a vtable. Greg mentioned something about the first child of the parent
  // having a byte_offset of 0 and being an instance variable.
  if (!symbol_name.startswith("vtable for ")) {
    m_error.SetErrorString("does not have a vtable");
    return false;
  }

  // Now that we know it's a vtable, we update the object's state.

  SetName(GetTypeName());

  // Calculate the number of entries
  assert(m_vtable_symbol->GetByteSizeIsValid());
  m_addr_size = process_sp->GetAddressByteSize();
  addr_t symbol_end_addr = m_vtable_symbol->GetLoadAddress(target_sp.get()) +
                           m_vtable_symbol->GetByteSize();
  m_num_vtable_entries = (symbol_end_addr - possible_vtable_ptr) / m_addr_size;

  m_value.SetValueType(Value::ValueType::LoadAddress);
  m_value.GetScalar() = possible_vtable_ptr;
  SetValueDidChange(true);
  SetValueIsValid(true);
  return true;
}

CompilerType ValueObjectVTable::GetCompilerTypeImpl() { return CompilerType(); }

ValueObjectVTable::~ValueObjectVTable() = default;
