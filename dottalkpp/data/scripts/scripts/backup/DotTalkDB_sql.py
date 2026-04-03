import subprocess
import os
import re
from typing import List, Dict, Optional, Any

class DotTalkDB:
    """
    A Python interface to the dottalk++ CLI, providing a SQL-like API.

    This class runs the C++ executable as a subprocess and communicates
    with it via standard input and output.
    """
    def __init__(self, dottalk_exe_path: str):
        """
        Initializes the connection to the dottalk++ executable.
        :param dottalk_exe_path: Path to the dottalk++ executable. Assumes it's in the PATH by default.
        """
        self.proc = None
        self.dottalk_exe_path = dottalk_exe_path
        self._start_process()

    def _start_process(self):
        """Starts the dottalk++ process and waits for the initial prompt."""
        try:
            # Use --quiet flag for a machine-readable interface.
            self.proc = subprocess.Popen(
                [self.dottalk_exe_path, '--quiet'],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                encoding='utf-8',
                bufsize=1,  # Line buffering
                universal_newlines=True
            )
            # Read the initial prompt to ensure readiness
            self._read_until_prompt()
            print("Connected to dottalk++ CLI.")
        except FileNotFoundError:
            print(f"Error: dottalk++ executable not found at '{self.dottalk_exe_path}'")
            self.proc = None
        except Exception as e:
            print(f"An unexpected error occurred during process startup: {e}", file=sys.stderr)
            self.proc = None

    def _read_until_prompt(self):
        """Reads all lines from stdout until the prompt is found."""
        output = []
        while self.proc.stdout:
            line = self.proc.stdout.readline()
            if not line:
                break
            if line.strip() == '.':
                break
            output.append(line)
        return "".join(output).strip()

    def _send_command(self, command: str) -> str:
        """Sends a command and reads all output until the next prompt."""
        if not self.proc or self.proc.poll() is not None:
            self._start_process()
            if not self.proc:
                return "Error: Process failed to restart."

        try:
            self.proc.stdin.write(command + '\n')
            self.proc.stdin.flush()
        except (IOError, BrokenPipeError) as e:
            self.proc.terminate()
            self.proc = None
            return f"Error writing to process stdin: {e}"

        return self._read_until_prompt()

    def _parse_select_result(self, output: str) -> List[Dict[str, Any]]:
        """Parses the output of a LIST command into a list of dictionaries."""
        lines = output.split('\n')
        
        header_line = None
        data_lines = []
        for line in lines:
            if re.match(r'^\s+\d+\s', line):
                data_lines.append(line)
            elif re.match(r'^\s+\*\s+\d+\s', line):
                data_lines.append(line)
            elif re.match(r'^\s+\d+\s+record', line):
                continue
            elif not header_line:
                header_line = line
        
        if not header_line:
            return []
            
        fields_and_widths = []
        start_index = 0
        
        header_parts = header_line.split()
        
        if len(header_parts) < 2: return []
        
        recno_header_end = header_line.find(header_parts[1]) + len(header_parts[1]) if len(header_parts) > 1 else 0
        data_header_start = header_line.find(header_parts[2]) if len(header_parts) > 2 else -1
        if data_header_start == -1: return []
        
        for field in header_parts[2:]:
            current_start = header_line.find(field, start_index)
            if current_start == -1: continue
            
            field_name = field.strip()
            field_width = len(field_name)
            
            next_space = header_line.find(' ', current_start + field_width)
            if next_space == -1:
                end_index = len(header_line)
            else:
                end_index = next_space
                
            fields_and_widths.append({'name': field_name, 'start': current_start, 'end': end_index})
            start_index = end_index + 1
            
        records = []
        for line in data_lines:
            record = {}
            for field_info in fields_and_widths:
                start = field_info['start']
                end = field_info['end']
                field_name = field_info['name']
                field_value = line[start:end].strip()
                record[field_name] = field_value
            records.append(record)
            
        return records

    def _parse_display_output(self, output: str) -> Optional[Dict[str, Any]]:
        """Parses the output of a DISPLAY command into a single dictionary."""
        record = {}
        for line in output.split('\n'):
            match = re.match(r'^\s+([^=]+)\s+=\s+(.*)$', line.strip())
            if match:
                field_name = match.group(1).strip()
                field_value = match.group(2).strip()
                record[field_name] = field_value
        return record if record else None

    def execute(self, command: str) -> str:
        """Sends a raw command to the dottalk++ shell and returns the raw output."""
        return self._send_command(command)

    def select(self, table: str, fields: str = "*", where: Optional[str] = None) -> List[Dict[str, Any]]:
        """
        Executes a SELECT-like command.
        :param table: The table to query.
        :param fields: The fields to return (e.g., "FIELD1, FIELD2"). Not yet implemented.
        :param where: A WHERE clause (e.g., "CITY = 'Portland'").
        :return: A list of dictionaries, one for each record.
        """
        self._send_command(f"USE {table}")

        if where:
            list_cmd = f"LIST ALL FOR {where}"
        else:
            list_cmd = "LIST ALL"
        
        output = self._send_command(list_cmd)
        
        return self._parse_select_result(output)

    def get_record_by_recno(self, table: str, recno: int) -> Optional[Dict[str, Any]]:
        """
        Retrieves a single record by its record number.
        :param table: The table to query.
        :param recno: The record number (1-based).
        :return: A dictionary representing the record, or None.
        """
        self._send_command(f"USE {table}")
        
        # Display command outputs a single record in a different format
        output = self._send_command(f"DISPLAY {recno}")
        
        # Parse the DISPLAY output, which is vertical (Field = Value)
        record = {}
        for line in output.split('\n'):
            match = re.match(r'^\s+([^=]+)\s+=\s+(.*)$', line.strip())
            if match:
                field_name = match.group(1).strip()
                field_value = match.group(2).strip()
                record[field_name] = field_value
        
        return record if record else None

    def insert(self, table: str, values: Dict[str, Any]):
        """
        Executes an INSERT-like command.
        :param table: The table to insert into.
        :param values: A dictionary of field-value pairs.
        """
        self._send_command(f"USE {table}")
        self._send_command("APPEND BLANK")

        for field, value in values.items():
            value_str = f'"{value}"' if isinstance(value, str) else str(value)
            self._send_command(f"REPLACE {field} WITH {value_str}")

        print("Record inserted.")
        
    def update(self, table: str, values: Dict[str, Any], where: str):
        """
        Executes an UPDATE-like command.
        :param table: The table to update.
        :param values: A dictionary of field-value pairs.
        :param where: A WHERE clause to identify records.
        """
        self._send_command(f"USE {table}")
        scan_loop = f"SCAN FOR {where}"
        for field, value in values.items():
            value_str = f'"{value}"' if isinstance(value, str) else str(value)
            scan_loop += f"\n  REPLACE {field} WITH {value_str}"
        scan_loop += "\nENDSCAN"
        self._send_command(scan_loop)
        print("Records updated.")

    def delete(self, table: str, where: str):
        """
        Executes a DELETE-like command.
        :param table: The table to delete from.
        :param where: A WHERE clause to identify records.
        """
        self._send_command(f"USE {table}")
        self._send_command(f"DELETE FOR {where}")
        print("Records deleted.")

    def close(self):
        """Closes the connection to the dottalk++ process."""
        if self.proc and self.proc.poll() is None:
            self.proc.stdin.write("QUIT\n")
            self.proc.stdin.flush()
            self.proc.wait(timeout=5)
            self.proc = None
            print("Connection to dottalk++ closed.")

def main():
    """Example usage of the DotTalkDB class."""
    db = DotTalkDB(os.path.join(os.getcwd(), "build", "Release", "dottalkpp.exe"))
    
    print("\n--- Listing fields for STUDENTS ---")
    output = db.execute("USE students")
    print(output)
    
    print("\n--- Retrieving a single record by RecNo ---")
    record_row = db.get_record_by_recno("students", 1)
    print(record_row)
    
    print("\n--- Listing all records from STUDENTS ---")
    users = db.select("students")
    print(users)

    print("\n--- Inserting a new record ---")
    new_user = {'LAST_NAME': 'Doe', 'FIRST_NAME': 'Jane', 'CITY': 'Eugene'}
    db.insert("students", new_user)
    
    print("\n--- Updating a record ---")
    db.update("students", {'GPA': 3.99}, where="FIRST_NAME = 'Jane'")

    print("\n--- Deleting a record ---")
    db.delete("students", where="FIRST_NAME = 'Jane'")

    db.close()

if __name__ == "__main__":
    main()
