//===----------------------------------------------------------------------===//
//
// Copyright (C) 2022 Sophgo Technologies Inc.  All rights reserved.
//
// TPU-MLIR is licensed under the 2-Clause BSD License except for the
// third-party components.
//
//===----------------------------------------------------------------------===//

#include "pycuda.h"

py_cuda::py_cuda() { CHECK_CUDNN(cudnnCreate(&cudnn_)); }

py_cuda::~py_cuda() { CHECK_CUDNN(cudnnDestroy(cudnn_)); }

void py_cuda::load(std::string filename) {
  if (context_) {
    llvm_unreachable("mlir can only load once");
  }
  DialectRegistry registry;
  registry.insert<func::FuncDialect, top::TopDialect, tpu::TpuDialect,
                  quant::QuantizationDialect>();
  context_ = std::make_unique<MLIRContext>(registry);

  module_ = parseSourceFile<ModuleOp>(filename, context_.get());
  assert(module_);
  auto m = module_.get();
  module::init(m);

  // load weight to cuda
  for (auto func : m.getOps<FuncOp>()) {
    func.walk([&](top::WeightOp weight) {
      cuda_malloc(weight_map_, weight);
      auto data = weight.read_as_byte();
      auto name = module::getName(weight.getOutput()).str();
      auto mem = weight_map_[name].get();
      CHECK_CUDA(
          cudaMemcpy(mem, data->data(), data->size(), cudaMemcpyHostToDevice));
    });
  }
  // perpare input and output
  std::vector<Value> inputs;
  std::vector<Value> outputs;
  module::getInputsOutputs(m, inputs, outputs);

  for (auto v : inputs) {
    auto name = module::getName(v).str();
    input_names.append(name);
    input_names_.push_back(name);
    cuda_malloc(activation_map_, v);
  }
  for (auto &v : outputs) {
    auto name = module::getName(v).str();
    output_names.append(name);
    output_names_.push_back(name);
    cuda_malloc(activation_map_, v);
    auto buffer =
        std::make_shared<std::vector<float>>(module::getNumElements(v));
    buffer_map_[name] = std::move(buffer);
  }
}

void py_cuda::cuda_malloc(std::map<std::string, cuda_ptr> &map, Value v) {
  auto name = module::getName(v).str();
  void *cuda_mem;
  CHECK_CUDA(cudaMalloc(&cuda_mem, module::getBytes(v)));
  cuda_ptr wrapper(cuda_mem);
  map[name] = std::move(wrapper);
  value_map_[name] = v;
}

cuda_ptr py_cuda::cuda_malloc(size_t bytes) {
  void *cuda_mem;
  CHECK_CUDA(cudaMalloc(&cuda_mem, bytes));
  cuda_ptr wrapper(cuda_mem);
  return std::move(wrapper);
}

// data in activation_map_ copy to buffer_map_; if it not float, convert to
// float first
void py_cuda::cuda_to_host(const std::string &name) {
  auto it_act = activation_map_.find(name);
  if (it_act == activation_map_.end()) {
    llvm_unreachable("name not find in activation");
  }
  auto cudaData = it_act->second.get();
  auto it_value = value_map_.find(name);
  if (it_value == value_map_.end()) {
    llvm_unreachable("name not find in value");
  }
  auto v = it_value->second;
  auto it_buffer = buffer_map_.find(name);
  if (buffer_map_.find(name) == buffer_map_.end()) {
    auto buffer =
        std::make_shared<std::vector<float>>(module::getNumElements(v));
    buffer_map_[name] = std::move(buffer);
    it_buffer = buffer_map_.find(name);
  }
  auto buffer = it_buffer->second->data();

  auto stype = module::getStorageType(v);
  if (stype.isF32()) {
    auto bytes = it_buffer->second->size() * sizeof(float);
    CHECK_CUDA(cudaMemcpy(buffer, cudaData, bytes, cudaMemcpyDeviceToHost));
  } else if (module::isUniformQuantized(v) && stype.isInteger(8)) {
    auto qtype = module::getUniformQuantizedType(v);
    auto num = module::getNumElements(v);
    uint8_t *temp = new uint8_t[num];
    CHECK_CUDA(cudaMemcpy(temp, cudaData, num, cudaMemcpyDeviceToHost));
    if (stype.isUnsignedInteger(8)) {
      for (int i = 0; i < num; i++) {
        buffer[i] = ((float)temp[i] - (float)qtype.getZeroPoint()) *
                    (float)qtype.getScale();
      }
    } else {
      int8_t *temp_i8 = (int8_t *)temp;
      for (int i = 0; i < num; i++) {
        buffer[i] = ((float)temp_i8[i] - (float)qtype.getZeroPoint()) *
                    (float)qtype.getScale();
      }
    }
    delete[] temp;
  } else {
    v.dump();
    llvm_unreachable("Not Implemented");
  }
}

void py_cuda::set_tensor(
    std::string name,
    py::array_t<float, py::array::c_style | py::array::forcecast> data) {
  auto it_value = value_map_.find(name);
  if (it_value == value_map_.end()) {
    llvm_unreachable("set_tensor name is not exist");
  }
  auto v = it_value->second;
  auto bytes = module::getBytes(v);
  if (data.size() * sizeof(float) != bytes) {
    llvm_unreachable("set_tensor data size is uncorrect");
  }
  void *dst = getCudaData(v);
  CHECK_CUDA(cudaMemcpy(dst, data.data(), bytes, cudaMemcpyHostToDevice));
}

void py_cuda::invoke(bool dump_all) {
  auto m = module_.get();
  for (auto func : m.getOps<FuncOp>()) {
    func.walk([&](Operation *op) {
      if (isa<top::NoneOp, top::InputOp, top::WeightOp, func::FuncOp,
              func::ReturnOp>(op)) {
        // do nothing
      } else {
        // 1. alloc output mem in cuda
        for (auto out : op->getResults()) {
          if (module::isNone(out)) {
            continue;
          }
          auto name = module::getName(out).str();
          if (activation_map_.find(name) != activation_map_.end()) {
            continue;
          }
          cuda_malloc(activation_map_, out);
        }
        // 2. inference
        if (auto tpuOp = dyn_cast<tpu::AddOp>(op)) {
          cudaAdd(tpuOp);
        } else if (auto tpuOp = dyn_cast<tpu::Conv2DOp>(op)) {
          cudaConv2D(tpuOp);
        } else if (auto tpuOp = dyn_cast<tpu::CastOp>(op)) {
          cudaCast(tpuOp);
        } else if (auto tpuOp = dyn_cast<tpu::GenericCpuOp>(op)) {
          cudaGenericCpu(tpuOp);
        } else if (auto tpuOp = dyn_cast<tpu::MatMulOp>(op)) {
          cudaMatMul(tpuOp);
        } else if (auto tpuOp = dyn_cast<tpu::ReshapeOp>(op)) {
          cudaReshape(tpuOp);
        } else {
          UNREACHABLE_OP("Not Implemented", op);
        }
        // 3. after inference, check input is still need, or remove from cuda
        for (auto in : op->getOperands()) {
          if (!module::isActive(in)) {
            continue;
          }
          auto name = module::getName(in).str();
          if (isa<top::InputOp>(in.getDefiningOp())) {
            continue;
          }
          bool need = false;
          for (auto u : in.getUsers()) {
            if (op->isBeforeInBlock(u)) {
              need = true;
              break;
            }
          }
          if (!need) {
            if (dump_all) {
              cudaDeviceSynchronize();
              cuda_to_host(name);
            }
            activation_map_.erase(name);
          }
        }
      }
    });
  }
  // after inference, copy result to buffer
  cudaDeviceSynchronize();
  for (auto &name : output_names_) {
    cuda_to_host(name);
  }
}

py::array py_cuda::get_tensor(std::string name) {
  auto it_buffer = buffer_map_.find(name);
  if (it_buffer == buffer_map_.end()) {
    std::cout << name << std::endl;
    llvm_unreachable("can't get_tensor name in buffer");
  }
  auto data = it_buffer->second;
  auto it_value = value_map_.find(name);
  if (it_value == value_map_.end()) {
    llvm_unreachable("can't get_tensor name in value");
  }
  auto shape = module::getShape(it_value->second);
  return getPyArray(std::move(data), shape);
}

py::dict py_cuda::get_all_tensor() {
  py::dict py_ret;
  for (auto &it : buffer_map_) {
    auto v = value_map_[it.first];
    auto shape = module::getShape(v);
    py::str py_s(it.first);
    py_ret[py_s] = getPyArray(it.second, shape);
  }
  return py_ret;
}
