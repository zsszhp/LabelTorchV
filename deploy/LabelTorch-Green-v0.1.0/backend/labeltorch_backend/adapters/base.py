"""
TrainingAdapter 抽象基类

定义训练后端的统一接口，任何训练框架必须实现此接口
"""
from abc import ABC, abstractmethod
from typing import Any


class TrainingAdapter(ABC):
    """训练适配器抽象基类"""

    @abstractmethod
    async def validate_config(self, config: dict) -> dict:
        """验证训练配置是否合法"""
        pass

    @abstractmethod
    async def prepare_dataset_snapshot(self, snapshot_path: str, data_yaml: str) -> dict:
        """准备训练数据快照"""
        pass

    @abstractmethod
    async def start_training(self, config: dict) -> dict:
        """启动训练"""
        pass

    @abstractmethod
    async def stop_training(self) -> dict:
        """停止训练"""
        pass

    @abstractmethod
    async def parse_logs(self, log_path: str) -> dict:
        """解析训练日志"""
        pass

    @abstractmethod
    async def collect_metrics(self, run_dir: str) -> dict:
        """收集训练指标"""
        pass

    @abstractmethod
    async def export_model(self, weight_path: str, format: str, options: dict) -> dict:
        """导出模型"""
        pass
