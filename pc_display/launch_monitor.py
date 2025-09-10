#!/usr/bin/env python3
"""
Simple launcher for FarDriver Monitor with better user feedback
"""

import sys
import os
import subprocess
import time

def main():
    print("=" * 60)
    print("FarDriver Monitor Launcher")
    print("=" * 60)
    
    # Check if we're in the right directory
    if not os.path.exists("FarDriver_Monitor.py"):
        print("ERROR: FarDriver_Monitor.py not found!")
        print("Please run this script from the pc_display directory.")
        input("Press Enter to exit...")
        return
    
    # Check dependencies
    print("Checking dependencies...")
    try:
        import bleak
        import tkinter
        print("✓ All dependencies are installed")
    except ImportError as e:
        print(f"✗ Missing dependency: {e}")
        print("Please install requirements: pip install -r requirements_FarDriver_Monitor.txt")
        input("Press Enter to exit...")
        return
    
    print("\nStarting FarDriver Monitor...")
    print("The GUI window should appear shortly.")
    print("If you don't see it:")
    print("  1. Check your taskbar for a Python icon")
    print("  2. Try Alt+Tab to cycle through windows")
    print("  3. Look for a window titled 'FarDriver Monitor'")
    print("\nPress Ctrl+C in this window to stop the program")
    print("-" * 60)
    
    try:
        # Launch the main program
        subprocess.run([sys.executable, "FarDriver_Monitor.py"], check=True)
    except KeyboardInterrupt:
        print("\nProgram stopped by user")
    except subprocess.CalledProcessError as e:
        print(f"\nError running program: {e}")
        input("Press Enter to exit...")
    except Exception as e:
        print(f"\nUnexpected error: {e}")
        input("Press Enter to exit...")

if __name__ == "__main__":
    main()
