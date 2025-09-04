# Ebike Data Parser

A Python script to parse and analyze ebike data from hex text files, with Excel export functionality.

## Features

- Parses hex data files containing ebike signals
- Identifies changes between consecutive signals
- Exports packet data to Excel with highlighting for changed values
- Supports multiple input file formats

## Installation

1. Clone this repository or download the files
2. Install required dependencies:
   ```bash
   pip install -r requirements.txt
   ```

## Usage

### Basic Usage

Run the parser with a text file containing hex data:

```bash
python3 svmc_parser.py SVMC72200-acc.txt
```

### Input File Format

The script expects text files containing hex data with the identifier `18F88006` separating individual signals.

### Output

- **Console Output**: Displays parsed signals with change indicators
- **Excel File**: Creates a spreadsheet (`*_packets.xlsx`) with:
  - Signal numbers in column A
  - Packet values (0-13) in columns B-O
  - Green highlighting on packets that changed from the previous signal
