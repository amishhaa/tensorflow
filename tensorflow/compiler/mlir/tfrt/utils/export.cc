/* Copyright 2023 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
#include "tensorflow/compiler/mlir/tfrt/utils/export.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"  // from @llvm-project
#include "mlir/IR/Builders.h"  // from @llvm-project
#include "mlir/IR/BuiltinOps.h"  // from @llvm-project
#include "mlir/Pass/PassManager.h"  // from @llvm-project
#include "tensorflow/compiler/mlir/tensorflow/transforms/passes.h"
#include "tensorflow/compiler/mlir/tensorflow/translate/export_graphdef.h"
#include "tensorflow/compiler/mlir/tensorflow/utils/error_util.h"
#include "tensorflow/core/framework/function.pb.h"
#include "tensorflow/tsl/profiler/lib/traceme.h"

namespace tensorflow {

absl::Status ExportFunctionDefs(
    mlir::ModuleOp module,
    absl::AnyInvocable<absl::Status(tensorflow::FunctionDef)> callback) {
  tsl::profiler::TraceMe traceme([&]() {
    return tsl::profiler::TraceMeEncode(
        "ExportFunctionDefs",
        {{"module_name", absl::string_view(module.getName().value_or("?"))}});
  });

  {
    mlir::StatusScopedDiagnosticHandler diag_handler(module.getContext());

    mlir::PassManager pm(module.getContext());

    mlir::TF::AddGraphExportLoweringPasses(pm);
    pm.addPass(mlir::CreateBreakUpIslandsPass());

    if (mlir::failed(pm.run(module))) {
      return diag_handler.ConsumeStatus();
    }
  }

  for (auto func : module.getOps<mlir::func::FuncOp>()) {
    tensorflow::FunctionDef function_def;
    RETURN_IF_ERROR(tensorflow::ConvertMlirFunctionToFunctionLibraryDef(
        func, tensorflow::GraphExportConfig(), &function_def));
    RETURN_IF_ERROR(callback(std::move(function_def)));
  }

  return absl::OkStatus();
}

}  // namespace tensorflow
