"""
导出命令处理器

支持 pt/onnx 导出，以及导出产物验证
"""
import logging
import os

logger = logging.getLogger(__name__)


async def handle_run(payload: dict) -> dict:
    """
    执行模型导出，根据选定的 adapter 进行动态导出
    """
    artifact_id = payload.get("artifact_id", "")
    weight_path = payload.get("weight_path", "")
    export_format = payload.get("format", "onnx")
    output_path = payload.get("output_path", "")
    options = payload.get("options", {})
    adapter_name = payload.get("adapter", "ultralytics")

    if not weight_path:
        return {
            "status": "failed",
            "error": {"code": "MISSING_PARAMS", "message": "Missing weight_path"},
            "artifact_id": artifact_id,
        }

    # 路径白名单：权重与导出产物路径必须落在 projectRoot / snapshotDir / 系统临时目录内
    from ..tools.path_security import (
        PathSecurityError,
        collect_allowed_roots,
        ensure_within,
    )

    read_roots = collect_allowed_roots(payload, extra_roots=[weight_path])
    write_roots = collect_allowed_roots(payload, include_data_dirs=False)

    checked_weight = ensure_within(weight_path, read_roots, field="weight_path", must_exist=True)
    if isinstance(checked_weight, dict):
        checked_weight["artifact_id"] = artifact_id
        return checked_weight
    weight_path = checked_weight

    if output_path:
        checked_output = ensure_within(output_path, write_roots, field="output_path", must_exist=False)
        if isinstance(checked_output, dict):
            checked_output["artifact_id"] = artifact_id
            return checked_output
        output_path = checked_output

    # options 内的 output_path 也一并校验，防止绕过顶层字段
    if isinstance(options, dict) and options.get("output_path"):
        checked_opt_out = ensure_within(options["output_path"], write_roots, field="options.output_path", must_exist=False)
        if isinstance(checked_opt_out, dict):
            checked_opt_out["artifact_id"] = artifact_id
            return checked_opt_out
        options = dict(options)
        options["output_path"] = checked_opt_out

    try:
        from ..adapters.registry import TrainingAdapterRegistry

        adapter_class = TrainingAdapterRegistry.get(adapter_name)
        if adapter_class is None:
            return {
                "status": "failed",
                "artifact_id": artifact_id,
                "error": {"code": "UNKNOWN_ADAPTER", "message": f"Unknown adapter: {adapter_name}"},
            }

        adapter = adapter_class()

        # 异步调用 adapter 的导出接口
        result = await adapter.export_model(weight_path, export_format, options)

        if result.get("status") == "succeeded":
            export_path_str = result.get("export_path", "")

            # 若输出路径与导出临时路径不同，拷贝至最终 output_path 目录
            if output_path and export_path_str and os.path.abspath(export_path_str) != os.path.abspath(output_path):
                os.makedirs(os.path.dirname(output_path), exist_ok=True)
                import shutil
                shutil.copy2(export_path_str, output_path)
                export_path_str = output_path

            file_size = os.path.getsize(export_path_str) if os.path.isfile(export_path_str) else 0

            return {
                "status": "succeeded",
                "artifact_id": artifact_id,
                "export_path": export_path_str,
                "format": export_format,
                "file_size_bytes": file_size,
            }
        else:
            err = result.get("error", "Export failed")
            return {
                "status": "failed",
                "artifact_id": artifact_id,
                "error": err if isinstance(err, dict) else {"code": "EXPORT_FAILED", "message": str(err)},
            }

    except PathSecurityError as e:
        err = e.to_error_dict()
        return {"status": "failed", "artifact_id": artifact_id, "error": err}
    except Exception as e:
        logger.error(f"Export failed: {e}")
        return {
            "status": "failed",
            "artifact_id": artifact_id,
            "error": {"code": "EXPORT_FAILED", "message": str(e)},
        }


async def handle_verify(payload: dict) -> dict:
    """
    验证导出产物

    payload:
        artifact_id: 导出产物ID
        artifact_path: 导出产物文件路径
        format: 导出格式（onnx / pt / torchscript / tflite / engine 等）

    返回约定（P0-3）：
        - 验证通过: {"status":"succeeded", "valid":true, ...}
        - 验证失败: {"status":"failed", "valid":false, "error":...}
        - 无验证器: {"valid":null, "verified":false, "reason":"no verifier"}
          C++ 侧不得据此标记为已验证成功，UI 显示「未验证」
    """
    artifact_id = payload.get("artifact_id", "")
    artifact_path = payload.get("output_path", "") or payload.get("artifact_path", "")
    artifact_format = payload.get("format", "onnx")

    if not artifact_path:
        return {
            "artifact_id": artifact_id,
            "status": "failed",
            "valid": False,
            "error": {"code": "MISSING_PARAMS", "message": "Missing artifact_path"},
        }

    # 路径白名单：待验证产物必须落在 projectRoot / snapshotDir / 系统临时目录内
    from ..tools.path_security import collect_allowed_roots, ensure_within

    allowed_roots = collect_allowed_roots(payload, extra_roots=[artifact_path])
    checked_artifact = ensure_within(artifact_path, allowed_roots, field="artifact_path", must_exist=True)
    if isinstance(checked_artifact, dict):
        checked_artifact["artifact_id"] = artifact_id
        checked_artifact["status"] = "failed"
        checked_artifact["valid"] = False
        return checked_artifact
    artifact_path = checked_artifact

    # 格式归一化：C++ 侧传 "pt"，后端 torchscript 校验器接受 pt|torchscript
    normalized_format = artifact_format.lower()
    if normalized_format == "pt":
        normalized_format = "torchscript"

    if normalized_format == "onnx":
        result = await _verify_onnx(artifact_path)
    elif normalized_format == "torchscript":
        result = await _verify_torchscript(artifact_path)
    else:
        # tflite / engine 等格式：无验证能力，明确返回未验证，禁止虚标 valid:true
        return {
            "artifact_id": artifact_id,
            "format": artifact_format,
            "valid": None,
            "verified": False,
            "reason": "no verifier",
        }

    # 有验证器的格式：根据验证结果设置 status 与 valid
    if result.get("valid"):
        result["status"] = "succeeded"
    else:
        result["status"] = "failed"
        # 确保失败时 valid 明确为 false（而非缺失）
        result["valid"] = False
        if "error" not in result:
            result["error"] = "Verification failed"

    result["artifact_id"] = artifact_id
    return result


async def _verify_onnx(artifact_path: str) -> dict:
    """使用 onnxruntime 验证 ONNX 模型（使用 asyncio.to_thread 避免阻塞）"""
    import asyncio
    return await asyncio.to_thread(_do_verify_onnx, artifact_path)


def _do_verify_onnx(artifact_path: str) -> dict:
    """使用 onnxruntime 验证 ONNX 模型"""
    try:
        import onnxruntime as ort

        session = ort.InferenceSession(artifact_path)
        inputs = session.get_inputs()
        outputs = session.get_outputs()

        input_info = []
        for inp in inputs:
            input_info.append({
                "name": inp.name,
                "shape": list(inp.shape),
                "type": str(inp.type),
            })

        output_info = []
        for out in outputs:
            output_info.append({
                "name": out.name,
                "shape": list(out.shape),
                "type": str(out.type),
            })

        del session

        return {
            "valid": True,
            "format": "onnx",
            "inputs": input_info,
            "outputs": output_info,
            "provider": "onnxruntime",
        }

    except ImportError:
        try:
            import onnx
            model = onnx.load(artifact_path)
            onnx.checker.check_model(model)

            graph = model.graph
            input_info = []
            for inp in graph.input:
                input_info.append({"name": inp.name})

            output_info = []
            for out in graph.output:
                output_info.append({"name": out.name})

            return {
                "valid": True,
                "format": "onnx",
                "inputs": input_info,
                "outputs": output_info,
                "provider": "onnx",
            }
        except ImportError:
            return {"valid": False, "error": "Neither onnxruntime nor onnx is installed"}
        except Exception as e:
            return {"valid": False, "error": f"ONNX validation failed: {e}"}

    except Exception as e:
        return {"valid": False, "error": f"ONNX Runtime validation failed: {e}"}


async def _verify_torchscript(artifact_path: str) -> dict:
    """验证 TorchScript 模型（使用 asyncio.to_thread 避免阻塞）"""
    import asyncio
    return await asyncio.to_thread(_do_verify_torchscript, artifact_path)


def _do_verify_torchscript(artifact_path: str) -> dict:
    """验证 TorchScript 模型"""
    try:
        import torch

        model = torch.jit.load(artifact_path, map_location="cpu")
        _code = model.code
        del model

        return {
            "valid": True,
            "format": "torchscript",
            "note": "TorchScript model loaded successfully",
        }

    except ImportError:
        return {"valid": False, "error": "PyTorch is not installed"}
    except Exception as e:
        return {"valid": False, "error": f"TorchScript validation failed: {e}"}
