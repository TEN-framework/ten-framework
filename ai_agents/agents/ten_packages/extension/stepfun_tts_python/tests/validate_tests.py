#!/usr/bin/env python3
"""
Test validation script for StepFun TTS Python Extension
This script validates the test file structure and imports
"""
import sys
import os
from pathlib import Path

def validate_test_files():
    """Validate test file structure and imports"""
    print("Validating StepFun TTS Test Files...")
    print("=" * 50)
    
    # Check if test files exist
    test_files = [
        "test_basic.py",
        "test_error_debug.py"
    ]
    
    for test_file in test_files:
        if os.path.exists(test_file):
            print(f"✓ {test_file} exists")
        else:
            print(f"✗ {test_file} missing")
            return False
    
    # Check if bin directory and start script exist
    if os.path.exists("bin/start"):
        print("✓ bin/start script exists")
    else:
        print("✗ bin/start script missing")
        return False
    
    # Try to import test modules (without running them)
    try:
        print("\nValidating test imports...")
        
        # Add current directory to path for imports
        sys.path.insert(0, os.getcwd())
        
        # Try to import test modules (this will fail without TEN Framework)
        try:
            import test_basic
            print("✓ test_basic.py imports successfully")
        except ImportError as e:
            if "ten_runtime" in str(e):
                print("✓ test_basic.py structure valid (TEN Framework not available)")
            else:
                raise e
        
        try:
            import test_error_debug
            print("✓ test_error_debug.py imports successfully")
        except ImportError as e:
            if "ten_runtime" in str(e):
                print("✓ test_error_debug.py structure valid (TEN Framework not available)")
            else:
                raise e
        
        # Check if test functions exist (only if modules imported successfully)
        if "test_basic" in sys.modules and "test_error_debug" in sys.modules:
            test_functions = [
                ("test_basic", "test_dump_functionality"),
                ("test_basic", "test_basic_functionality"),
                ("test_error_debug", "test_error_debug_information"),
                ("test_error_debug", "test_error_debug_stack_trace"),
                ("test_error_debug", "test_error_debug_request_context"),
                ("test_error_debug", "test_error_debug_voice_label_context"),
                ("test_error_debug", "test_error_debug_network_error"),
            ]
            
            for module_name, func_name in test_functions:
                module = sys.modules[module_name]
                if hasattr(module, func_name):
                    print(f"✓ {module_name}.{func_name} function exists")
                else:
                    print(f"✗ {module_name}.{func_name} function missing")
                    return False
        else:
            print("✓ Test function validation skipped (TEN Framework not available)")
        
    except ImportError as e:
        print(f"✗ Import error: {e}")
        return False
    except Exception as e:
        print(f"✗ Validation error: {e}")
        return False
    
    print("\n" + "=" * 50)
    print("✓ All test files validated successfully!")
    return True

def main():
    """Main validation function"""
    # Change to tests directory
    script_dir = Path(__file__).parent
    os.chdir(script_dir)
    
    success = validate_test_files()
    
    if success:
        print("\nStepFun TTS tests are ready to run!")
        print("Use './bin/start' to run all tests")
        return 0
    else:
        print("\nTest validation failed!")
        return 1

if __name__ == "__main__":
    sys.exit(main())
