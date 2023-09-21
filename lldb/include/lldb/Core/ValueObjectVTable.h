// TODO(toyang): make sure this name is consistent
//===-- ValueObjectVTable.h --------------------------------------*- C++
//-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_CORE_VALUEOBJECTVTABLE_H
#define LLDB_CORE_VALUEOBJECTVTABLE_H

#include "lldb/Core/ValueObject.h"

namespace lldb_private {

/// TODO(toyang):
class ValueObjectVTable : public ValueObject {
public:
  ~ValueObjectVTable() override;

  static lldb::ValueObjectSP Create(ValueObject &parent);

  std::optional<uint64_t> GetByteSize() override;

  size_t CalculateNumChildren(uint32_t max) override;

  ValueObject *CreateChildAtIndex(size_t idx, bool synthetic_array_member,
                                  int32_t synthetic_index) override;

  lldb::ValueType GetValueType() const override;

  ConstString GetTypeName() override;

  // Note: this is what `SBValue::GetTypeName()` calls.
  ConstString GetQualifiedTypeName() override;

  ConstString GetDisplayTypeName() override;

  bool IsInScope() override;

protected:
  bool UpdateValue() override;

  // TODO(toyang): symbol name?
  CompilerType GetCompilerTypeImpl() override;

  // // TODO: this is the load address of the vtable pointer, which points to
  // the first entry in the vtable (past the header). lldb::addr_t m_vtable_ptr
  // = LLDB_INVALID_ADDRESS;
  Symbol *m_vtable_symbol = nullptr;
  size_t m_num_vtable_entries = 0;
  size_t m_addr_size = 0;

private:
  ValueObjectVTable(ValueObject &parent);

  // For ValueObject only
  ValueObjectVTable(const ValueObjectVTable &) = delete;
  const ValueObjectVTable &operator=(const ValueObjectVTable &) = delete;
};

} // namespace lldb_private

#endif // LLDB_CORE_VALUEOBJECTVTABLE_H
