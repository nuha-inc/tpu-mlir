//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Support/MathUtils.h"
#include "tpu_mlir/Dialect/Tpu/Transforms/Codegen/Dynamic/DynamicLayer.hpp"
using namespace tpu_mlir::backend;

// =========================================
// GlobalGenInterface
// =========================================
void tpu::FAttentionOp::codegen_global_bm1684x() {
  auto op = getOperation();
  auto input_spec = BM168x::get_input_spec(op);
  auto output_spec = BM168x::get_output_spec(op);

  flash_attention_global_spec_t param = {0};
  auto &common = param.common;
  // get_param(op, common);
  common.batch = getBatch();
  common.head = getHead();
  common.mq = getMq();
  common.mk = getMk();
  common.dim = getDim();
  common.scale = getScale().convertToDouble();
  common.hasmask = !module::isNone(getMask());

  BM168x::call_global_func("backend_api_flash_attention_global", &param, sizeof(param),
                           input_spec->data(), output_spec->data());
}

int64_t tpu::FAttentionOp::get_fw_type_bm1684x() {
  return -1;
}

// ======================================
// Dynamic GlobalGenInterface
// ======================================
int64_t tpu::FAttentionOp::dyn_codegen_global_bm1684x(void *buffer) {
  llvm_unreachable("Not Implemented");
}