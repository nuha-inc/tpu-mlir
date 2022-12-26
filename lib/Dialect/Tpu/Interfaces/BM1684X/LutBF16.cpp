//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "tpu_mlir/Backend/BM168x/BM1684X.h"
#include "tpu_mlir/Dialect/Tpu/IR/TpuOps.h"
#include "tpu_mlir/Support/Helper/Module.h"
#include "tpu_mlir/Support/Helper/Quant.h"

using namespace mlir;
using namespace tpu_mlir;
using namespace tpu_mlir::helper;
using namespace tpu_mlir::backend;

// =========================================
// GlobalGenInterface
// =========================================

void tpu::LutBF16Op::codegen_global_bm1684x() {
  llvm_unreachable("Not Implemented");
}

// =========================================
// LocalGenInterface
// =========================================

int64_t tpu::LutBF16Op::getBufferSize_bm1684x(
    int64_t in_lmem_bytes, int64_t out_lmem_bytes, int64_t in_nslice,
    int64_t in_hslice, int64_t out_nslice, int64_t out_hslice) {
  return 0;
}

void tpu::LutBF16Op::assign_sec_info(int64_t n_step, int64_t h_step,
                                     void *sec_info_) {
  llvm_unreachable("Not Implemented");
}

void tpu::LutBF16Op::codegen_local_bm1684x(int64_t n_step, int64_t h_step,
                                           void *sec_info_) {
  llvm_unreachable("Not Implemented");
}
