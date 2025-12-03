import ctypes
import json
import os
import re
from typing import Optional, Any

class DotTalkCAPI:
    """
    Python wrapper for the DotTalk++ C++ library.
    This class handles the direct calls to the C++ shared library using ctypes.
    """
    def __init__(self, lib_path: Optional[str] = None):
        if not lib_path:
            # Assumes the shared library is in the build/Release directory relative to the project root
            # You may need to change this path depending on your build system
            build_dir = os.path.join(os.path.dirname(__file__), '..', '..', 'build', 'Release')
            lib_name = 'dottalkpp.dll' if os.name == 'nt' else 'libdottalkpp.so'
            lib_path = os.path.join(build_dir, lib_name)
        
        if not os.path.exists(lib_path):
            raise FileNotFoundError(f"Could not find the DotTalk++ shared library at: {lib_path}")
            
        self.lib = ctypes.CDLL(lib_path)
        
        # Define the C++ function signatures
        # void* CreateEngine();
        self.lib.CreateEngine.argtypes = []
        self.lib.CreateEngine.restype = ctypes.c_void_p

        # void DestroyEngine(void* engine_handle);
        self.lib.DestroyEngine.argtypes = [ctypes.c_void_p]
        self.lib.DestroyEngine.restype = None

        # char* ExecuteCommand(void* engine_handle, const char* command);
        self.lib.ExecuteCommand.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        self.lib.ExecuteCommand.restype = ctypes.c_char_p

        # char* GetLastOutput(void* engine_handle);
        self.lib.GetLastOutput.argtypes = [ctypes.c_void_p]
        self.lib.GetLastOutput.restype = ctypes.c_char_p

    def create_engine(self):
        return self.lib.CreateEngine()

    def destroy_engine(self, engine_handle):
        self.lib.DestroyEngine(engine_handle)

    def execute_command(self, engine_handle, command: str) -> str:
        command_bytes = command.encode('utf-8')
        result_ptr = self.lib.ExecuteCommand(engine_handle, command_bytes)
        if result_ptr:
            # Get the output from the last command and decode
            output = self.lib.GetLastOutput(engine_handle).decode('utf-8')
            return output
        return ""

class DotTalkEngine:
    """
    High-level Python wrapper for the DotTalk++ engine.
    This class provides a clean, machine-friendly interface.
    """
    def __init__(self, lib_path: Optional[str] = None):
        self.api = DotTalkCAPI(lib_path)
        self.engine_handle = self.api.create_engine()
        self.last_output = ""

    def __del__(self):
        if self.engine_handle:
            self.api.destroy_engine(self.engine_handle)

    def _parse_output(self, output: str) -> dict:
        # A simple parser to turn human-readable output into a dictionary
        # This is where we standardize the data
        output = output.strip()
        
        # Example: 'Replaced GPA at recno 5.'
        if output.startswith("Replaced"):
            m = re.search(r"Replaced (.*?) at recno (\d+).", output)
            if m:
                return {"status": "ok", "action": "replace", "field": m.group(1), "recno": int(m.group(2))}
        
        # Example: 'Opened students.dbf.'
        if output.startswith("Opened"):
            m = re.search(r"Opened (.*?).", output)
            if m:
                return {"status": "ok", "action": "open", "file": m.group(1)}
        
        # You will need to add more parsing rules for other commands like CREATE, DELETE, APPEND, etc.
        # This is a key part of the standardization process
        return {"status": "ok", "message": output}

    def execute(self, command: str) -> dict:
        self.last_output = self.api.execute_command(self.engine_handle, command)
        return self._parse_output(self.last_output)

    def get_last_output(self) -> str:
        return self.last_output

if __name__ == "__main__":
    # Example usage
    try:
        engine = DotTalkEngine()
        print("Engine initialized.")
        
        # Example command:
        result = engine.execute('USE students.dbf')
        print(f"Result of 'USE students.dbf': {result}")
        
        # This part assumes you have a students.dbf with at least one record
        # and that the command works in your C++ core
        result = engine.execute('REPLACE GPA WITH 4.00')
        print(f"Result of 'REPLACE GPA WITH 4.00': {result}")
        
    except FileNotFoundError as e:
        print(e)
    except Exception as e:
        print(f"An unexpected error occurred: {e}")
