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
        return {"status": "failed", "error": "Missing weight_path"}

    if not os.path.isfile(weight_path):
        return {"status": "failed", "error": f"Weight file not found: {weight_path}"}

    try:
        from ..adapters.registry import TrainingAdapterRegistry

        adapter_class = TrainingAdapterRegistry.get(adapter_name)
        if adapter_class is None:
            return {"status": "failed", "error": f"Unknown adapter: {adapter_name}"}

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
            return {
                "status": "failed",
                "artifact_id": artifact_id,
                "error": result.get("error", "Export failed"),
            }

    except Exception as e:
        logger.error(f"Export failed: {e}")
        return {"status": "failed", "artifact_id": artifact_id, "error": str(e)}


async def handle_verify(payload: dict) -> dict:
    """
    验证导出产物

    payload:
        artifact_id: 导出产物ID
        artifact_path: 导出产物文件路径
        format: 导出格式
    """
    artifact_id = payload.get("artifact_id", "")
    artifact_path = payload.get("output_path", "") or payload.get("artifact_path", "")
    artifact_format = payload.get("format", "onnx")

    if not artifact_path:
        return {"artifact_id": artifact_id, "valid": False, "error": "Missing artifact_path"}

    if not os.path.isfile(artifact_path):
        return {"artifact_id": artifact_id, "valid": False, "error": f"File not found: {artifact_path}"}

    if artifact_format == "onnx":
        result = await _verify_onnx(artifact_path)
    elif artifact_format == "torchscript":
        result = await _verify_torchscript(artifact_path)
    else:
        result = {"valid": True, "format": artifact_format, "note": "No verification available for this format"}

    result["artifact_id"] = artifact_id
    return result


async def _verify_onnx(artifact_path: str) -> dict:
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
