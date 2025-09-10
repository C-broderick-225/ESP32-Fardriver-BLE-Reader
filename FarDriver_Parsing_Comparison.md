# FarDriver BLE Data Parsing Comparison

## Overview

This document compares the data parsing implementations between the Arduino ESP32 program (`FardriverBLE.ino`) and the Python PC display program (`FarDriver_Monitor.py`) for FarDriver BLE controller communication.

## Executive Summary

The Python program is **approximately 70% correct** but has several critical issues that would cause incorrect data interpretation. The Arduino program appears to be the authoritative reference implementation with proper address mapping and byte positioning.

## Key Differences

### 1. Address Mapping System

#### Arduino Implementation ✅
```cpp
const uint8_t flash_read_addr[55] = {
  0xE2, 0xE8, 0xEE, 0xE4, 0x06, 0x0C, 0x12, 0xE2, 0xE8, 0xEE, 0x18, 0x1E, 0x24, 0x2A,
  0xE2, 0xE8, 0xEE, 0x30, 0x5D, 0x63, 0x69, 0xE2, 0xE8, 0xEE, 0x7C, 0x82, 0x88, 0x8E,
  0xE2, 0xE8, 0xEE, 0x94, 0x9A, 0xA0, 0xA6, 0xE2, 0xE8, 0xEE, 0xAC, 0xB2, 0xB8, 0xBE,
  0xE2, 0xE8, 0xEE, 0xC4, 0xCA, 0xD0, 0xE2, 0xE8, 0xEE, 0xD6, 0xDC, 0xF4, 0xFA
};

// Maps packet ID to memory address
uint8_t id = packet[1] & 0x3F;
uint8_t address = flash_read_addr[id];
```

#### Python Implementation ❌
```python
# Simple index-based parsing without address mapping
index = data[1]
if index == 0:  # Main data
elif index == 1:  # Voltage
elif index == 4:  # Controller temperature
elif index == 13:  # Motor temperature and throttle
```

**Issue**: Python lacks the sophisticated address mapping system that correctly maps packet IDs to specific memory addresses.

### 2. Data Byte Positions

#### RPM Parsing

| Program | Byte Position | Code |
|---------|---------------|------|
| **Arduino** ✅ | Bytes 8-9 | `(pData[7] << 8) \| pData[6]` |
| **Python** ❌ | Bytes 4-5 | `(data[4] << 8) \| data[5]` |

**Issue**: Python reads RPM from the wrong byte positions.

#### Gear Parsing

| Program | Byte Position | Code |
|---------|---------------|------|
| **Arduino** ✅ | Byte 2, bits 2-3 | `(pData[0] >> 2) & 0b11` |
| **Python** ✅ | Byte 2, bits 2-3 | `(data[2] >> 2) & 0x03` |

**Status**: Both programs correctly parse gear information.

#### Voltage Parsing

| Program | Byte Position | Code |
|---------|---------------|------|
| **Arduino** ✅ | Bytes 2-3 | `((pData[1] << 8) \| pData[0]) / 10.0f` |
| **Python** ✅ | Bytes 2-3 | `((data[2] << 8) \| data[3]) / 10.0` |

**Status**: Both programs correctly parse voltage (note byte order difference).

### 3. Current and Power Calculation

#### Arduino Implementation ✅
```cpp
// Current parsing (0xE8 packet)
float new_lineCurrent = (int16_t)((pData[5] << 8) | pData[4]) / 4.0f;
// Power calculation
controllerData.power = controllerData.voltage * controllerData.lineCurrent;
```

#### Python Implementation ❌
```python
# Current parsing (index 0)
iq = ((data[8] << 8) | data[9]) / 100.0
id = ((data[10] << 8) | data[11]) / 100.0
is_mag = (iq * iq + id * id) ** 0.5
power = -is_mag * ctr_data.voltage
```

**Issues**:
- Different byte positions for current data
- Different scaling factors (/4.0 vs /100.0)
- Different calculation methods (direct vs magnitude)

### 4. Temperature Parsing

#### Controller Temperature

| Program | Address/Packet | Byte Position | Code |
|---------|----------------|---------------|------|
| **Arduino** ✅ | 0xD6 | Bytes 12-13 | `(int16_t)((pData[11] << 8) \| pData[10])` |
| **Python** ❌ | Index 4 | Byte 2 | `data[2]` |

#### Motor Temperature

| Program | Address/Packet | Byte Position | Code |
|---------|----------------|---------------|------|
| **Arduino** ✅ | 0xF4 | Bytes 2-3 | `(int16_t)((pData[1] << 8) \| pData[0])` |
| **Python** ❌ | Index 13 | Byte 2 | `data[2]` |

**Issues**: Python reads temperatures from wrong byte positions and doesn't handle 16-bit values.

### 5. CRC/Checksum Validation

#### Arduino Implementation ✅
```cpp
bool verifyCRC(uint8_t* data, uint16_t length) {
  if (length != 16) return false;
  uint16_t crc = 0x7F3C;
  for (int i = 0; i < 14; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
  }
  return crc == ((data[15] << 8) | data[14]);
}
```

#### Python Implementation ❌
```python
# Simple XOR checksum
expected_checksum = 0
for i in range(1, 14):
    expected_checksum ^= data[i]
checksum_valid = checksum == expected_checksum
```

**Issue**: Python uses simple XOR instead of proper CRC-16 validation.

### 6. Missing Data Fields

#### State of Charge (SOC)
- **Arduino** ✅: Parses SOC from 0xF4 packet (`controllerData.soc = pData[3]`)
- **Python** ❌: No SOC parsing implemented

#### Regeneration Detection
- **Arduino** ✅: Detects regen from negative current (`controllerData.isRegenFromCurrent = (controllerData.lineCurrent < 0)`)
- **Python** ❌: No explicit regen detection

### 7. Speed Calculation

#### Arduino Implementation ✅
```cpp
const float wheel_circumference_m = 1.416;
const int motor_pole_pairs = 20;
controllerData.rpm = displayRawRpm * 4.0 / motor_pole_pairs;
controllerData.speed = (controllerData.rpm * wheel_circumference_m * 60.0) / 1000.0;
```

#### Python Implementation ❌
```python
wheel_circumference = 1.350  # meters
rear_wheel_rpm = rpm / 4.0
distance_per_min = rear_wheel_rpm * wheel_circumference
speed = distance_per_min * 0.06  # km/h
```

**Issues**:
- Different wheel circumference values
- Different calculation methods
- Missing motor pole pairs consideration

## Critical Issues Summary

### High Priority Issues
1. **RPM Byte Position**: Python reads from bytes 4-5, should be bytes 8-9
2. **Address Mapping**: Python needs to implement the flash_read_addr system
3. **Current Parsing**: Wrong byte positions and scaling factors
4. **Temperature Parsing**: Wrong byte positions for both controller and motor temps

### Medium Priority Issues
1. **CRC Validation**: Implement proper CRC-16 instead of XOR
2. **SOC Parsing**: Add State of Charge parsing from 0xF4 packets
3. **Speed Calculation**: Align with Arduino's calculation method

### Low Priority Issues
1. **Regen Detection**: Add explicit regeneration detection
2. **Data Validation**: Add range checking like Arduino implementation

## Recommendations

### Immediate Actions Required
1. **Implement Address Mapping**: Add the `flash_read_addr` array to Python
2. **Fix Byte Positions**: Align RPM and current parsing with Arduino positions
3. **Update Temperature Parsing**: Use correct byte positions for temperature data
4. **Add CRC Validation**: Implement proper CRC-16 checksum validation

### Code Changes Needed

#### 1. Add Address Mapping
```python
flash_read_addr = [
    0xE2, 0xE8, 0xEE, 0xE4, 0x06, 0x0C, 0x12, 0xE2, 0xE8, 0xEE, 0x18, 0x1E, 0x24, 0x2A,
    0xE2, 0xE8, 0xEE, 0x30, 0x5D, 0x63, 0x69, 0xE2, 0xE8, 0xEE, 0x7C, 0x82, 0x88, 0x8E,
    0xE2, 0xE8, 0xEE, 0x94, 0x9A, 0xA0, 0xA6, 0xE2, 0xE8, 0xEE, 0xAC, 0xB2, 0xB8, 0xBE,
    0xE2, 0xE8, 0xEE, 0xC4, 0xCA, 0xD0, 0xE2, 0xE8, 0xEE, 0xD6, 0xDC, 0xF4, 0xFA
]
```

#### 2. Fix RPM Parsing
```python
# Change from:
rpm = (data[4] << 8) | data[5]
# To:
rpm = (data[8] << 8) | data[9]
```

#### 3. Fix Current Parsing
```python
# Change from:
iq = ((data[8] << 8) | data[9]) / 100.0
# To:
lineCurrent = (int16_t)((data[5] << 8) | data[4]) / 4.0
```

#### 4. Add CRC Validation
```python
def verify_crc(data, length):
    if length != 16:
        return False
    crc = 0x7F3C
    for i in range(14):
        crc ^= data[i]
        for j in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else crc >> 1
    return crc == ((data[15] << 8) | data[14])
```

## Conclusion

The Python program needs significant updates to match the Arduino's parsing logic for accurate data translation. The Arduino program should be considered the authoritative reference implementation. Priority should be given to fixing the byte position issues and implementing the address mapping system.

## Testing Recommendations

1. **Compare Raw Data**: Log identical packets from both programs to verify byte positions
2. **Validate Calculations**: Test with known values to ensure calculations match
3. **Check CRC**: Verify that CRC validation works correctly
4. **Test Edge Cases**: Ensure proper handling of negative values and boundary conditions

---

*Document created: $(date)*
*Last updated: $(date)*
