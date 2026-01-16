def parse_key_value_block(text: str):
    """Parse key-value pairs from dumpsys output."""
    data = {}
    for line in text.splitlines():
        if ":" in line:
            k, v = line.split(":", 1)
            key = k.strip()
            val = v.strip()
            # Skip empty keys and values
            if key and val:
                data[key] = val
    return data


def parse_cpu_freq(text: str):
    """Parse CPU frequency from scaling_cur_freq files."""
    freqs = {}
    for line in text.splitlines():
        if ":" in line:
            path, value = line.split(":", 1)
            try:
                cpu = path.split("/")[-2]
                freqs[cpu] = int(value.strip())
            except (ValueError, IndexError):
                continue
    return freqs if freqs else {"error": "Could not parse CPU frequencies"}


def parse_thermal_data(text: str):
    """Parse thermal service output and extract temperatures."""
    temps = {}
    for line in text.splitlines():
        if "Temperature{" in line:
            # Extract temperature data from format: Temperature{mValue=39.5, mType=0, mName=AP, mStatus=0}
            try:
                parts = line.split("Temperature{")[1].rstrip("}")
                items = [item.strip() for item in parts.split(", ")]
                temp_data = {}
                for item in items:
                    if "=" in item:
                        k, v = item.split("=", 1)
                        temp_data[k.strip()] = v.strip()
                
                if "mName" in temp_data:
                    name = temp_data["mName"]
                    temps[name] = {
                        "value": float(temp_data.get("mValue", 0)),
                        "type": temp_data.get("mType", ""),
                        "status": temp_data.get("mStatus", "")
                    }
            except (IndexError, ValueError):
                continue
    return temps if temps else {"raw": text[:500]}


def parse_battery_level(data: dict) -> dict:
    """Extract key battery metrics from parsed data."""
    return {
        "level": int(data.get("level", 0)),
        "health": data.get("health", "unknown"),
        "status": data.get("status", "unknown"),
        "voltage_mv": int(data.get("voltage", 0)),
        "temperature_c": round(int(data.get("temperature", 0)) / 10, 1),
        "technology": data.get("technology", "unknown"),
        "is_charging": data.get("AC powered", "").lower() == "true" or 
                       data.get("USB powered", "").lower() == "true"
    }


def kb_to_mb(value: str) -> float:
    """Convert KB to MB."""
    try:
        return round(int(value) / 1024, 2)
    except ValueError:
        return 0.0


def kb_to_gb(value: int) -> float:
    """Convert KB to GB."""
    return round(value / (1024 * 1024), 2)
