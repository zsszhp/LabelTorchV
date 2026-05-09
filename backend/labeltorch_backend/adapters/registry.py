"""
Training adapter registry.

Allows registration and discovery of custom training adapters.
Built-in adapters (UltralyticsAdapter) are registered by default.
"""
import logging
from typing import Dict, Type
from .base import TrainingAdapter

logger = logging.getLogger(__name__)


class TrainingAdapterRegistry:
    """Registry for training adapter plugins."""
    
    _adapters: Dict[str, Type[TrainingAdapter]] = {}
    
    @classmethod
    def register(cls, name: str, adapter_class: Type[TrainingAdapter]):
        """Register a training adapter by name."""
        if name in cls._adapters:
            logger.warning(f"Overwriting existing adapter: {name}")
        cls._adapters[name] = adapter_class
        logger.info(f"Registered training adapter: {name}")
    
    @classmethod
    def get(cls, name: str) -> Type[TrainingAdapter]:
        """Get a registered adapter class by name."""
        return cls._adapters.get(name)
    
    @classmethod
    def list_adapters(cls) -> list:
        """List all registered adapter names."""
        return list(cls._adapters.keys())
    
    @classmethod
    def is_registered(cls, name: str) -> bool:
        """Check if an adapter is registered."""
        return name in cls._adapters


# Register built-in adapters
def register_builtin_adapters():
    """Register all built-in training adapters."""
    from .ultralytics_adapter import UltralyticsAdapter
    
    TrainingAdapterRegistry.register("ultralytics", UltralyticsAdapter)
    # Additional built-in adapters can be registered here
