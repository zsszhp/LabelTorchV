"""
LabelTorch Environment Self-Check

Run this on first launch to verify the runtime environment is complete.
"""
import sys
import os

def check_environment():
    """Check if the runtime environment is complete."""
    errors = []
    warnings = []

    # Check Python version
    if sys.version_info < (3, 11):
        errors.append(f"Python 3.11+ required, found {sys.version}")
    else:
        print(f"[OK] Python {sys.version_info.major}.{sys.version_info.minor}")

    # Check required packages
    required_packages = ["ultralytics"]
    for pkg in required_packages:
        try:
            __import__(pkg)
            print(f"[OK] Package: {pkg}")
        except ImportError:
            warnings.append(f"Missing package: {pkg} (install with: pip install {pkg})")

    # Check CUDA availability
    try:
        import torch
        if torch.cuda.is_available():
            print(f"[OK] CUDA available: {torch.cuda.get_device_name(0)}")
        else:
            print("[INFO] CUDA not available (CPU mode)")
    except ImportError:
        warnings.append("PyTorch not installed (required for GPU training)")

    # Print summary
    if errors:
        print("\n=== ERRORS ===")
        for e in errors:
            print(f"  ERROR: {e}")

    if warnings:
        print("\n=== WARNINGS ===")
        for w in warnings:
            print(f"  WARNING: {w}")

    if not errors and not warnings:
        print("\nAll checks passed!")

    return len(errors) == 0

if __name__ == "__main__":
    check_environment()
