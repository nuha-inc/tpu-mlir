//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Dialect/Tpu/Transforms/Codegen/Dynamic/DynamicLayer.hpp"
using namespace tpu_mlir::backend;

// ======================================
// GlobalGenInterface
// ======================================
void tpu::ShapeArithOp::codegen_global_bm1684x() {
  llvm_unreachable("Not supported now");
}

// ======================================
// Dynamic GlobalGenInterface
// ======================================
int64_t tpu::ShapeArithOp::dyn_codegen_global_bm1684x(void *buffer) {
  if (buffer) {
    std::string op_type = getType().str();

    // TODO: Min Max GT LT GE LE SQRT ...
    if (op_type == "Add")
      return BM168x::dynamic_spec_to_buffer(buffer, 0);
    else if (op_type == "Sub")
      return BM168x::dynamic_spec_to_buffer(buffer, 1);
    else if (op_type == "Sub")
      return BM168x::dynamic_spec_to_buffer(buffer, 2);
    else if (op_type == "Sub")
      return BM168x::dynamic_spec_to_buffer(buffer, 3);
  }
  return sizeof(int);
}

int64_t tpu::ShapeArithOp::get_fw_type_bm1684x() {
  return FW_BMNET_SHAPE_ARITH;
}

mlir::Type tpu::ShapeArithOp::type_verify(uint64_t opd_idx, TypeCastMode &mode) {
  return do_nothing(mode);
}