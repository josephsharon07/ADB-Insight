#!/usr/bin/env python3
"""
WebSocket client examples for ADB-Insight real-time metrics streaming.

Usage:
    python3 websocket_client.py metrics        # Stream general metrics
    python3 websocket_client.py cpu            # Stream CPU frequencies
"""

import asyncio
import websockets
import json
import sys
from datetime import datetime


async def connect_metrics(uri="ws://localhost:8000/ws/metrics"):
    """Connect to real-time metrics WebSocket stream."""
    print(f"Connecting to {uri}...")
    async with websockets.connect(uri) as websocket:
        print("✓ Connected to metrics stream")
        print("Streaming real-time metrics (Ctrl+C to stop)...\n")
        
        try:
            while True:
                data = await websocket.recv()
                metrics = json.loads(data)
                
                if "error" in metrics:
                    print(f"❌ Error: {metrics['error']}")
                else:
                    print(f"[{metrics['timestamp']}]")
                    print(f"  Battery:  {metrics['battery_level']}%")
                    print(f"  Memory:   {metrics['memory_usage_percent']:.1f}%")
                    print(f"  Storage:  {metrics['storage_usage_percent']:.1f}%")
                    print(f"  CPU Avg:  {metrics['cpu_avg_mhz']:.0f} MHz")
                    print(f"  CPU Min:  {metrics['cpu_min_mhz']:.0f} MHz")
                    print(f"  CPU Max:  {metrics['cpu_max_mhz']:.0f} MHz")
                    print(f"  Max Temp: {metrics['thermal_max_temp']:.1f}°C")
                    print()
        except KeyboardInterrupt:
            print("\n✓ Disconnected from metrics stream")


async def connect_cpu(uri="ws://localhost:8000/ws/cpu"):
    """Connect to real-time CPU frequency WebSocket stream."""
    print(f"Connecting to {uri}...")
    async with websockets.connect(uri) as websocket:
        print("✓ Connected to CPU frequency stream")
        print("Streaming CPU frequencies (Ctrl+C to stop)...\n")
        
        try:
            while True:
                data = await websocket.recv()
                metrics = json.loads(data)
                
                if "error" in metrics:
                    print(f"❌ Error: {metrics['error']}")
                else:
                    print(f"[{metrics['timestamp']}]")
                    print(f"  Cores: {metrics['core_count']}")
                    print(f"  Min/Max/Avg: {metrics['min_mhz']:.0f} / {metrics['max_mhz']:.0f} / {metrics['avg_mhz']:.0f} MHz")
                    print("  Per-core frequencies:")
                    for cpu, freq in sorted(metrics['per_core'].items()):
                        freq_mhz = freq / 1000
                        print(f"    {cpu}: {freq_mhz:.0f} MHz")
                    print()
        except KeyboardInterrupt:
            print("\n✓ Disconnected from CPU stream")


async def main():
    if len(sys.argv) < 2:
        print("Usage: python3 websocket_client.py [metrics|cpu] [host:port]")
        print("\nExamples:")
        print("  python3 websocket_client.py metrics")
        print("  python3 websocket_client.py cpu")
        print("  python3 websocket_client.py cpu 10.10.10.48:8000")
        return
    
    stream_type = sys.argv[1]
    host_port = sys.argv[2] if len(sys.argv) > 2 else "localhost:8000"
    
    if stream_type == "metrics":
        uri = f"ws://{host_port}/ws/metrics"
        await connect_metrics(uri)
    elif stream_type == "cpu":
        uri = f"ws://{host_port}/ws/cpu"
        await connect_cpu(uri)
    else:
        print(f"Unknown stream type: {stream_type}")
        print("Use 'metrics' or 'cpu'")


if __name__ == "__main__":
    asyncio.run(main())
