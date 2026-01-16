import subprocess
import logging
from typing import Optional

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

def adb_shell(cmd: str) -> str:
    """
    Run an adb shell command and return stdout.
    Raises RuntimeError if command fails.
    """
    try:
        result = subprocess.check_output(
            ["adb", "shell", cmd],
            stderr=subprocess.PIPE,
            text=True,
            timeout=10
        ).strip()
        logger.info(f"ADB command executed: {cmd[:50]}...")
        return result
    except subprocess.TimeoutExpired:
        logger.error(f"ADB command timeout: {cmd}")
        raise RuntimeError("Command timed out")
    except subprocess.CalledProcessError as e:
        logger.error(f"ADB command failed: {cmd} - {e.stderr}")
        raise RuntimeError(f"ADB command failed: {e.stderr}")
    except FileNotFoundError:
        logger.error("ADB not found. Ensure ADB is installed and in PATH")
        raise RuntimeError("ADB not found. Ensure ADB is installed and in PATH")


def adb_devices() -> Optional[str]:
    """Check connected ADB devices."""
    try:
        result = subprocess.check_output(
            ["adb", "devices"],
            stderr=subprocess.PIPE,
            text=True,
            timeout=5
        ).strip()
        return result
    except Exception as e:
        logger.error(f"Failed to check devices: {e}")
        return None
