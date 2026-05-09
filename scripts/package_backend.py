"""
Package the Python backend for distribution.

Creates a self-contained Python environment with all dependencies.
"""
import subprocess
import sys
import os
import shutil

def package_backend(output_dir="deploy/LabelTorch/backend"):
    """Package the Python backend."""
    backend_dir = os.path.join(os.path.dirname(__file__), "..", "backend")

    # Install requirements into a venv
    venv_dir = os.path.join(output_dir, "venv")

    print("Creating virtual environment...")
    subprocess.run([sys.executable, "-m", "venv", venv_dir], check=True)

    # Install requirements
    pip = os.path.join(venv_dir, "Scripts", "pip.exe")
    requirements = os.path.join(backend_dir, "requirements.txt")

    print("Installing dependencies...")
    subprocess.run([pip, "install", "-r", requirements], check=True)

    # Copy backend source
    print("Copying backend source...")
    dest_backend = os.path.join(output_dir, "labeltorch_backend")
    if os.path.exists(dest_backend):
        shutil.rmtree(dest_backend)
    shutil.copytree(
        os.path.join(backend_dir, "labeltorch_backend"),
        dest_backend
    )

    print(f"Backend packaged to: {output_dir}")

if __name__ == "__main__":
    package_backend()
