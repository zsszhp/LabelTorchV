"""
导出命令处理器

支持 pt/onnx 导出，以及导出产物验证
"""
import asyncio
import logging
import os

logger = logging.getLogger(__name__)


async def handle_run(payload: dict) -> dict:
    """
    执行模型导出

    payload:
        weight_path: 模型权重文件路径 (best.pt / last.pt)
        format: 导出格式 (onnx / torchscript / openvino)
        options: 导出选项 (imgsz, opset, dynamic, simplify, etc.)
    """
    weight_path = payload.get("weight_path", "")
    export_format = payload.get("format", "onnx")
    options = payload.get("options", {})

    if not weight_path:
        return {"status": "failed", "error": "Missing weight_path"}

    if not os.path.isfile(weight_path):
        return {"status": "failed", "error": f"Weight file not found: {weight_path}"}

    try:
        from ultralytics import YOLO

        model = YOLO(weight_path)

        loop = asyncio.get_event_loop()
        export_path = await loop.run_in_executor(
            None,
            lambda: model.export(
                format=export_format,
                imgsz=options.get("imgsz", 640),
                opset=options.get("opset", 13),
                dynamic=options.get("dynamic", True),
                simplify=options.get("simplify", True),
                half=options.get("half", False),
                int8=options.get("int8", False),
            )
        )

        export_path_str = str(export_path)
        file_size = os.path.getsize(export_path_str) if os.path.isfile(export_path_str) else 0

        logger.info(f"Export completed: {export_path_str} ({file_size} bytes)")

        return {
            "status": "succeeded",
            "export_path": export_path_str,
            "format": export_format,
            "file_size_bytes": file_size,
        }

    except ImportError:
        return {"status": "failed", "error": "Ultralytics is not installed"}
    except Exception as e:
        logger.error(f"Export failed: {e}")
        return {"status": "failed", "error": str(e)}


async def handle_verify(payload: dict) -> dict:
    """
    验证导出产物

    payload:
        artifact_path: 导出产物文件路径
        format: 导出格式
    """
    artifact_path = payload.get("artifact_path", "")
    artifact_format = payload.get("format", "onnx")

    if not artifact_path:
        return {"valid": False, "error": "Missing artifact_path"}

    if not os.path.isfile(artifact_path):
        return {"valid": False, "error": f"File not found: {artifact_path}"}

    if artifact_format == "onnx":
        return await _verify_onnx(artifact_path)
    elif artifact_format == "torchscript":
        return await _verify_torchscript(artifact_path)
    else:
        return {"valid": True, "format": artifact_format, "note": "No verification available for this format"}


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

        code = model.code
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
