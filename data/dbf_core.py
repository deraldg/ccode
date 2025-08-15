# dbf_core.py

import struct
import datetime
import os
import textwrap
import sys

# --- Constants from XBASE.H ---
HEADER_TERM_BYTE = 0x0D
IS_DELETED = 0x2A
NOT_DELETED = 0x20
BLOCK_SIZE = 32

# --- HeaderRec Class ---
class HeaderRec:
    """Represents the header record of a DBF file."""
    # < (little-endian)
    # B (unsigned char - version)
    # 3s (3-byte string - last_updated YYMMDD)
    # L (unsigned long - num_of_recs)
    # H (unsigned short - data_start)
    # H (unsigned short - chars_per_rec)
    # 20s (20-byte string - reserved)
    HEADER_STRUCT_FORMAT = '<B3sLHH20s'
    HEADER_SIZE = 32

    def __init__(self,
                 version: int = 0x03,
                 last_updated: bytes | datetime.date = b'\x00\x00\x00',
                 num_of_recs: int = 0,
                 data_start: int = 0,
                 chars_per_rec: int = 0,
                 reserved: bytes = b'\x00' * 20):
        self.version = version
        self.last_updated = last_updated
        self.num_of_recs = num_of_recs
        self.data_start = data_start
        self.chars_per_rec = chars_per_rec
        self.reserved = reserved

    @classmethod
    def from_bytes(cls, data: bytes):
        """Creates a HeaderRec instance from 32 bytes of data."""
        if len(data) != cls.HEADER_SIZE:
            raise ValueError(f"Header data must be {cls.HEADER_SIZE} bytes long.")
        (version, last_updated_bytes, num_of_recs,
         data_start, chars_per_rec, reserved) = struct.unpack(cls.HEADER_STRUCT_FORMAT, data)

        instance = cls(version, last_updated_bytes, num_of_recs, data_start, chars_per_rec, reserved)
        instance._parse_last_updated_date()
        return instance

    def to_bytes(self) -> bytes:
        """Converts the HeaderRec instance to 32 bytes for writing."""
        if isinstance(self.last_updated, datetime.date):
            year_byte = self.last_updated.year - 1900
            month_byte = self.last_updated.month
            day_byte = self.last_updated.day
            last_updated_bytes = bytes([year_byte, month_byte, day_byte])
        elif isinstance(self.last_updated, bytes) and len(self.last_updated) == 3:
            last_updated_bytes = self.last_updated
        else:
            print(f"Warning: last_updated has unexpected type {type(self.last_updated)}. Using default bytes.")
            last_updated_bytes = b'\x00\x00\x00'

        return struct.pack(self.HEADER_STRUCT_FORMAT,
                           self.version,
                           last_updated_bytes,
                           self.num_of_recs,
                           self.data_start,
                           self.chars_per_rec,
                           self.reserved)

    def _parse_last_updated_date(self):
        """Parses the 3-byte last_updated field into a datetime.date object."""
        if isinstance(self.last_updated, bytes) and len(self.last_updated) == 3:
            try:
                year = 1900 + self.last_updated[0]
                month = self.last_updated[1]
                day = self.last_updated[2]
                if 1 <= month <= 12 and 1 <= day <= 31:
                    self.last_updated = datetime.date(year, month, day)
                else:
                    pass
            except ValueError:
                pass

    def __repr__(self):
        return (f"HeaderRec(version=0x{self.version:02X}, last_updated={self.last_updated}, "
                f"num_of_recs={self.num_of_recs}, data_start={self.data_start}, "
                f"chars_per_rec={self.chars_per_rec})")

# --- FieldRec Class ---
class FieldRec:
    """Represents a field descriptor in a DBF file."""
    # < (little-endian)
    # 11s (11-byte string - field_name)
    # c (char - field_type)
    # L (unsigned long - field_data_address)
    # B (unsigned char - field_length)
    # B (unsigned char - decimal_places)
    # H (unsigned short - multiuser1)
    # B (unsigned char - workid)
    # H (unsigned short - multiuser2)
    # B (unsigned char - flag)
    # 8s (8-byte string - reserved)
    FIELD_STRUCT_FORMAT = '<11scLBBHBHB8s'
    FIELD_SIZE = 32

    def __init__(self,
                 field_name: bytes,
                 field_type: bytes,
                 field_data_address: int,
                 field_length: int,
                 decimal_places: int,
                 multiuser1: int,
                 workid: int,
                 multiuser2: int,
                 flag: int,
                 reserved: bytes):
        self.field_name = field_name.rstrip(b'\x00 ').decode('ascii')
        self.field_type = field_type.decode('ascii')
        self.field_data_address = field_data_address
        self.field_length = field_length
        self.decimal_places = decimal_places
        self.multiuser1 = multiuser1
        self.multiuser2 = multiuser2
        self.workid = workid
        self.flag = flag
        self.reserved = reserved

    @classmethod
    def from_bytes(cls, data: bytes):
        """Creates a FieldRec instance from 32 bytes of data."""
        if len(data) != cls.FIELD_SIZE:
            raise ValueError(f"Field data must be {cls.FIELD_SIZE} bytes long.")
        (field_name, field_type, field_data_address,
         field_length, decimal_places, multiuser1,
         workid, multiuser2, flag, reserved) = struct.unpack(cls.FIELD_STRUCT_FORMAT, data)
        return cls(field_name, field_type, field_data_address,
                   field_length, decimal_places, multiuser1,
                   workid, multiuser2, flag, reserved)

    def to_bytes(self) -> bytes:
        """Converts the FieldRec instance to 32 bytes for writing."""
        encoded_name = self.field_name.encode('ascii').ljust(11, b' ')
        return struct.pack(self.FIELD_STRUCT_FORMAT,
                           encoded_name,
                           self.field_type.encode('ascii'),
                           self.field_data_address,
                           self.field_length,
                           self.decimal_places,
                           self.multiuser1,
                           self.workid,
                           self.multiuser2,
                           self.flag,
                           self.reserved)

    def __repr__(self):
        return (f"FieldRec(name='{self.field_name}', type='{self.field_type}', "
                f"length={self.field_length}, dec={self.decimal_places})")

# --- DbfArea Class ---
class DbfArea:
    """
    Represents a single open DBF file (equivalent to C's select_rec).
    Manages the file pointer, header, field definitions, and current record.
    """
    def __init__(self, area_id: int):
        self.area_id = area_id
        self.fp: 'io.BufferedReader' = None
        self.file_ok: bool = False
        self.header: HeaderRec = None
        self.fields: list[FieldRec] = []
        self.current_rec_num: int = 0
        self.record_buffer: bytes = b''
        self.is_deleted: bool = False
        self._parsed_record_data: dict = {}
        self.indexes: list = []
        self.num_of_fields: int = 0

    def open_file(self, filename: str) -> bool:
        """
        Opens the DBF file.
        """
        try:
            self.fp = open(filename, 'rb+')
            self.file_ok = True
            print(f"File '{filename}' opened successfully for area {self.area_id}.")
            return True
        except IOError as e:
            print(f"Error opening file '{filename}': {e}")
            self.file_ok = False
            return False

    def close_file(self) -> bool:
        """
        Closes the DBF file and cleans up resources.
        """
        if self.fp:
            try:
                self.fp.close()
                self.fp = None
                self.file_ok = False
                self.header = None
                self.fields = []
                self.current_rec_num = 0
                self.record_buffer = b''
                self.is_deleted = False
                self._parsed_record_data = {}
                self.num_of_fields = 0
                print(f"File for area {self.area_id} closed successfully.")
                return True
            except IOError as e:
                print(f"Error closing file for area {self.area_id}: {e}")
                return False
        return False

    def read_header(self) -> bool:
        """
        Reads the DBF file header.
        """
        if not self.fp or not self.file_ok:
            print(f"File not open for area {self.area_id}.")
            return False

        self.fp.seek(0)
        header_bytes = self.fp.read(HeaderRec.HEADER_SIZE)
        if len(header_bytes) != HeaderRec.HEADER_SIZE:
            print(f"Failed to read full header for area {self.area_id}.")
            return False

        self.header = HeaderRec.from_bytes(header_bytes)

        self.num_of_fields = (self.header.data_start - HeaderRec.HEADER_SIZE - 1) // FieldRec.FIELD_SIZE
        return True

    def read_fields(self) -> bool:
        """
        Reads all field descriptors after the header.
        """
        if not self.fp or not self.file_ok or not self.header:
            print(f"File not ready for field reading for area {self.area_id}.")
            return False

        self.fp.seek(HeaderRec.HEADER_SIZE)
        self.fields = []

        for i in range(self.num_of_fields):
            field_bytes = self.fp.read(FieldRec.FIELD_SIZE)
            if len(field_bytes) != FieldRec.FIELD_SIZE:
                print(f"Failed to read field {i+1} for area {self.area_id}.")
                return False
            field_rec = FieldRec.from_bytes(field_bytes)
            self.fields.append(field_rec)

        terminator_byte = self.fp.read(1)
        if not terminator_byte or terminator_byte[0] != HEADER_TERM_BYTE:
            print(f"Warning: Header terminator byte not found or incorrect for area {self.area_id}.")
            pass

        self.current_rec_num = 0
        return True

    def read_rec(self) -> dict:
        """
        Reads the current record into the record_buffer and parses it.
        """
        if not self.fp or not self.file_ok or not self.header or not self.fields:
            print(f"File or metadata not ready for reading record for area {self.area_id}.")
            self._parsed_record_data = {}
            return {}

        if not (1 <= self.current_rec_num <= self.header.num_of_recs):
            self._parsed_record_data = {}
            return {}

        record_offset = self.header.data_start + (self.current_rec_num - 1) * self.header.chars_per_rec
        self.fp.seek(record_offset)

        full_record_bytes = self.fp.read(self.header.chars_per_rec)
        if len(full_record_bytes) != self.header.chars_per_rec:
            print(f"Failed to read full record {self.current_rec_num} for area {self.area_id}.")
            self._parsed_record_data = {}
            return {}

        self.is_deleted = (full_record_bytes[0] == IS_DELETED)
        self.record_buffer = full_record_bytes[1:]

        self._parsed_record_data = self._parse_record_buffer()
        return self._parsed_record_data

    def _parse_record_buffer(self) -> dict:
        """
        Parses the raw record_buffer into a dictionary based on field definitions.
        """
        parsed_data = {}
        offset = 0
        for field in self.fields:
            field_data_bytes = self.record_buffer[offset : offset + field.field_length]
            field_value = None

            try:
                if field.field_type == 'C':
                    field_value = field_data_bytes.decode('ascii').strip()
                elif field.field_type == 'N':
                    numeric_str = field_data_bytes.decode('ascii').strip()
                    if numeric_str:
                        if field.decimal_places > 0:
                            field_value = float(numeric_str)
                        else:
                            field_value = int(numeric_str)
                    else:
                        field_value = 0
                elif field.field_type == 'D':
                    date_str = field_data_bytes.decode('ascii').strip()
                    if len(date_str) == 8:
                        try:
                            field_value = datetime.datetime.strptime(date_str, '%Y%m%d').date()
                        except ValueError:
                            field_value = None
                    else:
                        field_value = None
                elif field.field_type == 'L':
                    logical_char = field_data_bytes.decode('ascii').strip().upper()
                    if logical_char in ('T', 'Y'):
                        field_value = True
                    elif logical_char in ('F', 'N'):
                        field_value = False
                    else:
                        field_value = None
                elif field.field_type == 'M':
                    field_value = field_data_bytes.decode('ascii').strip()
                else:
                    field_value = field_data_bytes.decode('ascii').strip()
            except (UnicodeDecodeError, ValueError) as e:
                print(f"Error parsing field '{field.field_name}': {e}. Raw: {field_data_bytes}")
                field_value = None

            parsed_data[field.field_name] = field_value
            offset += field.field_length
        return parsed_data

    def goto_rec(self, rec_num: int) -> bool:
        """
        Sets the current record number and reads that record.
        """
        if not self.file_ok or not self.header:
            print(f"File not open or header not read for area {self.area_id}.")
            return False

        if not (1 <= rec_num <= self.header.num_of_recs):
            print(f"Record number {rec_num} out of bounds (1 to {self.header.num_of_recs}) for area {self.area_id}.")
            return False

        self.current_rec_num = rec_num
        return self.read_rec() is not None

    def write_rec(self) -> bool:
        """
        Writes the current parsed record data back to the DBF file.
        """
        if not self.fp or not self.file_ok or not self.header or not self.fields:
            print(f"File or metadata not ready for writing record for area {self.area_id}.")
            return False

        if not (1 <= self.current_rec_num <= self.header.num_of_recs):
            print(f"Cannot write: Invalid record number {self.current_rec_num} for area {self.area_id}.")
            return False

        try:
            reconstructed_record_data_bytes = self._reconstruct_record_buffer()
        except ValueError as e:
            print(f"Error reconstructing record for writing: {e}")
            return False

        delete_byte = bytes([IS_DELETED if self.is_deleted else NOT_DELETED])
        full_record_bytes = delete_byte + reconstructed_record_data_bytes

        if len(full_record_bytes) != self.header.chars_per_rec:
            print(f"Error: Reconstructed record length ({len(full_record_bytes)}) does not match expected ({self.header.chars_per_rec}).")
            return False

        record_offset = self.header.data_start + (self.current_rec_num - 1) * self.header.chars_per_rec
        self.fp.seek(record_offset)

        bytes_written = self.fp.write(full_record_bytes)
        if bytes_written != self.header.chars_per_rec:
            print(f"Warning: Only {bytes_written} of {self.header.chars_per_rec} bytes written for record {self.current_rec_num}.")
            return False

        self.fp.flush()
        return True

    def _reconstruct_record_buffer(self) -> bytes:
        """
        Reconstructs the raw byte string for a record from the parsed_record_data dictionary
        and field definitions.
        """
        if not self.fields or not self._parsed_record_data:
            raise ValueError("No field definitions or parsed record data available to reconstruct.")

        record_bytes_parts = []
        for field in self.fields:
            field_value = self._parsed_record_data.get(field.field_name)
            field_data_bytes = b''

            if field_value is None:
                field_value = ""

            if field.field_type == 'C':
                s_val = str(field_value).encode('ascii', errors='replace')
                field_data_bytes = s_val.ljust(field.field_length, b' ')
            elif field.field_type == 'N':
                if field.decimal_places > 0:
                    format_str = f"{{:{field.field_length}.{field.decimal_places}f}}"
                    try:
                        s_val = format_str.format(float(field_value)).encode('ascii')
                    except ValueError:
                        s_val = b' ' * field.field_length
                else:
                    try:
                        s_val = str(int(field_value)).rjust(field.field_length, ' ').encode('ascii')
                    except ValueError:
                        s_val = b' ' * field.field_length
                field_data_bytes = s_val.rjust(field.field_length, b' ')
            elif field.field_type == 'D':
                if isinstance(field_value, datetime.date):
                    s_val = field_value.strftime('%Y%m%d').encode('ascii')
                else:
                    try:
                        date_obj = datetime.datetime.strptime(str(field_value), '%Y%m%d').date()
                        s_val = date_obj.strftime('%Y%m%d').encode('ascii')
                    except (ValueError, TypeError):
                        s_val = b' ' * 8
                field_data_bytes = s_val.ljust(field.field_length, b' ')
            elif field.field_type == 'L':
                if field_value is True:
                    s_val = b'T'
                elif field_value is False:
                    s_val = b'F'
                else:
                    s_val = b'?'
                field_data_bytes = s_val.ljust(field.field_length, b' ')
            elif field.field_type == 'M':
                s_val = str(field_value).encode('ascii', errors='replace')
                field_data_bytes = s_val.ljust(field.field_length, b' ')
            else:
                s_val = str(field_value).encode('ascii', errors='replace')
                field_data_bytes = s_val.ljust(field.field_length, b' ')

            if len(field_data_bytes) > field.field_length:
                field_data_bytes = field_data_bytes[:field.field_length]
            elif len(field_data_bytes) < field.field_length:
                field_data_bytes = field_data_bytes.ljust(field.field_length, b' ')

            record_bytes_parts.append(field_data_bytes)

        return b''.join(record_bytes_parts)

    def add_rec(self) -> bool:
        """
        Adds a new, blank record to the end of the DBF file.
        """
        if not self.fp or not self.file_ok or not self.header or not self.fields:
            print(f"File or metadata not ready to add record for area {self.area_id}.")
            return False

        blank_record_data = bytes([NOT_DELETED]) + (b' ' * (self.header.chars_per_rec - 1))

        self.fp.seek(0, os.SEEK_END)
        current_eof_pos = self.fp.tell()

        if current_eof_pos > 0:
            self.fp.seek(current_eof_pos - 1)
            last_byte = self.fp.read(1)
            if last_byte == b'\x1A':
                self.fp.seek(current_eof_pos - 1)
            else:
                self.fp.seek(current_eof_pos)
        else:
             self.fp.seek(0)

        bytes_written = self.fp.write(blank_record_data)
        if bytes_written != self.header.chars_per_rec:
            print(f"Error: Failed to write full new record.")
            return False

        self.fp.write(b'\x1A')
        self.fp.flush()

        self.header.num_of_recs += 1
        self.fp.seek(0)
        header_bytes_to_write = self.header.to_bytes()
        self.fp.write(header_bytes_to_write)
        self.fp.flush()

        self.current_rec_num = self.header.num_of_recs
        self.is_deleted = False
        self.record_buffer = blank_record_data[1:]
        self._parsed_record_data = self._parse_record_buffer()

        return True

    def del_rec(self) -> bool:
        """
        Marks the current record as deleted or undeleted.
        """
        if not self.fp or not self.file_ok or not self.header:
            print(f"File or metadata not ready to mark record for area {self.area_id}.")
            return False

        if not (1 <= self.current_rec_num <= self.header.num_of_recs):
            print(f"Cannot mark: Invalid record number {self.current_rec_num} for area {self.area_id}.")
            return False

        record_start_offset = self.header.data_start + (self.current_rec_num - 1) * self.header.chars_per_rec
        self.fp.seek(record_start_offset)

        new_delete_flag = bytes([IS_DELETED if not self.is_deleted else NOT_DELETED])
        self.fp.write(new_delete_flag)
        self.fp.flush()

        self.is_deleted = not self.is_deleted
        return True

    def create_dbf(self, filename: str, field_definitions: list[dict]) -> bool:
        """
        Creates a new DBF file with the specified field definitions.
        """
        if self.file_ok:
            print(f"Error: Area {self.area_id} already has an open file. Close it first.")
            return False

        try:
            self.fp = open(filename, 'wb+')
            self.file_ok = True

            self.fields = []
            current_data_address = 1
            total_record_length = 1

            for fd in field_definitions:
                name = fd.get('name', '')
                type_ = fd.get('type', 'C').upper()
                length = int(fd.get('length', 10))
                dec = int(fd.get('dec', 0)) if type_ == 'N' else 0

                if not (1 <= length <= 254):
                    print(f"Warning: Field '{name}' has invalid length {length}. Clamping to 254.")
                    length = min(length, 254)

                if type_ == 'D': length = 8
                if type_ == 'L': length = 1
                if type_ == 'M': length = 10

                field_rec = FieldRec(
                    field_name=name.encode('ascii').ljust(11, b' '),
                    field_type=type_.encode('ascii'),
                    field_data_address=current_data_address,
                    field_length=length,
                    decimal_places=dec,
                    multiuser1=0, multiuser2=0, workid=0, flag=0, reserved=b'\x00' * 8
                )
                self.fields.append(field_rec)
                current_data_address += length
                total_record_length += length

            self.num_of_fields = len(self.fields)

            self.header = HeaderRec()
            self.header.version = 0x03
            self.header.last_updated = datetime.date.today()
            self.header.num_of_recs = 0
            self.header.data_start = HeaderRec.HEADER_SIZE + (self.num_of_fields * FieldRec.FIELD_SIZE) + 1
            self.header.chars_per_rec = total_record_length

            self.fp.write(self.header.to_bytes())
            for field_rec in self.fields:
                self.fp.write(field_rec.to_bytes())

            self.fp.write(bytes([HEADER_TERM_BYTE]))
            self.fp.write(b'\x1A')
            self.fp.flush()

            self.current_rec_num = 0
            return True

        except IOError as e:
            print(f"Error creating file '{filename}': {e}")
            self.file_ok = False
            if self.fp: self.fp.close()
            return False
        except Exception as e:
            print(f"An unexpected error occurred during DBF creation: {e}")
            if self.fp: self.fp.close()
            return False

# --- Main DbfInstance Class ---
class DbfInstance:
    """
    The main database instance, acting as an 'umbrella' for DBF areas.
    """
    def __init__(self):
        self._areas: dict[int, DbfArea] = {}
        self._active_area_id: int = 0
        self.GRERROR: int = 0
        self.DOSERROR: int = 0
        self.IBMPC: bool = True

        self._initialize_areas()

        self.commands = {
            "help": self.do_help,
            "open": self.do_open,
            "close": self.do_close,
            "select": self.do_select,
            "goto": self.do_goto,
            "display": self.do_display,
            "info": self.do_info,
            "set": self.do_set,
            "write": self.do_write,
            "add": self.do_add,
            "delete": self.do_delete,
            "undelete": self.do_undelete,
            "create": self.do_create,
            "fields": self.do_fields,
            "quit": self.do_quit,
            "exit": self.do_quit,
        }

    def _initialize_areas(self, max_areas: int = 25):
        """
        Initializes DbfArea objects.
        """
        for i in range(1, max_areas + 1):
            self._areas[i] = DbfArea(area_id=i)
        self._active_area_id = 1

    @property
    def current_area(self) -> DbfArea:
        """Returns the currently active DbfArea object."""
        if self._active_area_id == 0:
            print("Error: No active database area selected.")
            return None
        area = self._areas.get(self._active_area_id)
        if area and area.file_ok:
            return area
        elif area and not area.file_ok:
            return area
        else:
            print(f"Error: Area {self._active_area_id} does not exist.")
            return None

    def select_area(self, area_id: int) -> bool:
        """Selects a specific area to make it active."""
        if area_id in self._areas:
            self._active_area_id = area_id
            print(f"Selected area {area_id}.")
            return True
        else:
            print(f"Error: Area {area_id} does not exist.")
            return False

    # --- CLI "do_" Verbs ---

    def do_help(self, *args):
        """
        Displays available commands and their usage.
        Usage: HELP [command]
        """
        if args and args[0] in self.commands:
            cmd_func = self.commands[args[0]]
            if cmd_func.__doc__:
                print(f"\nCommand: {args[0].upper()}")
                print(textwrap.fill(textwrap.dedent(cmd_func.__doc__).strip(), width=70))
            else:
                print(f"No specific help available for '{args[0]}'.")
        else:
            print("\n" + textwrap.dedent("""
            XBASE.PY - DBF File Utility (Python Port)
            Available Commands:
            """).strip())
            max_cmd_len = max(len(cmd) for cmd in self.commands.keys())
            for cmd, func in sorted(self.commands.items()):
                doc_summary = textwrap.dedent(func.__doc__ or "").strip().split('\n')[0]
                print(f"  {cmd.ljust(max_cmd_len)} - {doc_summary}")
            print(textwrap.dedent("""
            Type HELP <command> for more details.
            """).strip())

    def do_open(self, filename: str, area_id_str: str = None):
        """
        Opens a DBF file.
        Usage: OPEN <filename> [area_id]
        If area_id is not provided, uses the currently active area.
        """
        target_area_id = self._active_area_id
        if area_id_str:
            try:
                target_area_id = int(area_id_str)
            except ValueError:
                print(f"Error: Invalid area ID '{area_id_str}'. Must be a number.")
                return

        if not self.select_area(target_area_id):
            return

        current_area = self._areas.get(target_area_id)
        if not current_area:
             return

        if current_area.file_ok:
            print(f"Area {target_area_id} already has a file open. Close it first.")
            return

        print(f"Attempting to open '{filename}' in area {target_area_id}...")
        if not current_area.open_file(filename):
            self.GRERROR = 1
            print("Error: Could not open file.")
            return

        if not current_area.read_header():
            current_area.close_file()
            self.GRERROR = 1
            print("Error: Could not read header.")
            return

        if not current_area.read_fields():
            current_area.close_file()
            self.GRERROR = 1
            print("Error: Could not read fields.")
            return

        self.GRERROR = 0
        print(f"File '{filename}' successfully opened in area {target_area_id}.")

    def do_close(self, area_id_str: str = None):
        """
        Closes the currently active DBF file or a specified area.
        Usage: CLOSE [area_id]
        """
        target_area_id = self._active_area_id
        if area_id_str:
            try:
                target_area_id = int(area_id_str)
            except ValueError:
                print(f"Error: Invalid area ID '{area_id_str}'. Must be a number.")
                return

        if target_area_id not in self._areas:
            print(f"Error: Area {target_area_id} does not exist.")
            return

        if not self._areas[target_area_id].file_ok:
            print(f"Warning: Area {target_area_id} has no open file to close.")
            return

        if self._areas[target_area_id].close_file():
            print(f"File in area {target_area_id} closed.")
        else:
            print(f"Failed to close file in area {target_area_id}.")

    def do_select(self, area_id_str: str):
        """
        Selects a DBF area to make it active.
        Usage: SELECT <area_id>
        """
        try:
            area_id = int(area_id_str)
            self.select_area(area_id)
        except ValueError:
            print(f"Error: Invalid area ID '{area_id_str}'. Must be a number.")
        except IndexError:
            print("Usage: SELECT <area_id>")

    def do_goto(self, rec_num_str: str):
        """
        Moves to a specific record number in the currently active DBF file and reads it.
        Usage: GOTO <record_number>
        """
        current_area = self.current_area
        if not current_area:
            return

        try:
            rec_num = int(rec_num_str)
            if current_area.goto_rec(rec_num):
                print(f"Moved to record {current_area.current_rec_num}.")
            else:
                print(f"Failed to go to record {rec_num}. (Record out of bounds or read error)")
        except ValueError:
            print(f"Error: Invalid record number '{rec_num_str}'. Must be a number.")
        except IndexError:
            print("Usage: GOTO <record_number>")

    def do_display(self, *args):
        """
        Displays the current record's data.
        Usage: DISPLAY [FIELD_NAME]
        If FIELD_NAME is provided, displays only that field.
        """
        current_area = self.current_area
        if not current_area or not current_area._parsed_record_data:
            print("No record loaded in the current active area.")
            return

        record_data = current_area._parsed_record_data
        field_to_display = args[0].upper() if args else None

        if field_to_display:
            found_field_name = None
            for key in record_data.keys():
                if key.upper() == field_to_display:
                    found_field_name = key
                    break

            if found_field_name:
                print(f"{found_field_name}: {record_data[found_field_name]}")
            else:
                print(f"Field '{field_to_display}' not found in the current record.")
        else:
            print(f"\n--- Record {current_area.current_rec_num} (Deleted: {current_area.is_deleted}) ---")
            for field_name, value in record_data.items():
                print(f"{field_name}: {value}")
            print("--------------------------------------")

    def do_info(self, *args):
        """
        Displays information about the current DBF file and active area.
        Usage: INFO
        """
        print("\n--- DBF Instance Info ---")
        print(f"Active Area ID: {self._active_area_id}")
        print(f"GRERROR: {self.GRERROR}")
        print(f"DOSERROR: {self.DOSERROR}")
        print(f"IBMPC: {self.IBMPC}")

        current_area = self._areas.get(self._active_area_id)
        if current_area:
            print(f"\n--- Area {current_area.area_id} Info ---")
            print(f"File Open: {current_area.file_ok}")
            if current_area.file_ok and current_area.header:
                print(f"  Filename: {current_area.fp.name}")
                print(f"  DBF Version: 0x{current_area.header.version:02X}")
                print(f"  Last Updated: {current_area.header.last_updated}")
                print(f"  Total Records: {current_area.header.num_of_recs}")
                print(f"  Data Start Offset: {current_area.header.data_start} bytes")
                print(f"  Chars per Record: {current_area.header.chars_per_rec} bytes")
                print(f"  Number of Fields: {current_area.num_of_fields}")
                print(f"  Current Record #: {current_area.current_rec_num}")
            else:
                print("  No file open in this area.")
        else:
            print("No active area object found.")
        print("--------------------------")

    def do_set(self, field_name: str, *new_value_parts):
        """
        Sets the value of a field in the current record.
        Usage: SET <field_name> <new_value>
        Requires a record to be loaded (e.g., using GOTO or ADD).
        After setting, you should use 'WRITE' to save changes to the file.
        """
        current_area = self.current_area
        if not current_area or not current_area._parsed_record_data:
            print("No active area or record loaded. Use GOTO or ADD first.")
            return

        new_value_str = ' '.join(new_value_parts)
        if not new_value_str and len(new_value_parts) == 0:
            print("Error: A new value must be provided.")
            self.do_help("set")
            return

        field_obj = None
        for field in current_area.fields:
            if field.field_name.upper() == field_name.upper():
                field_obj = field
                break

        if not field_obj:
            print(f"Error: Field '{field_name}' not found in the current DBF structure.")
            print(f"Available fields: {[f.field_name for f in current_area.fields]}")
            return

        converted_value = None
        try:
            if field_obj.field_type == 'C':
                converted_value = str(new_value_str)
            elif field_obj.field_type == 'N':
                if field_obj.decimal_places > 0:
                    converted_value = float(new_value_str)
                else:
                    converted_value = int(new_value_str)
            elif field_obj.field_type == 'D':
                if '-' in new_value_str:
                    converted_value = datetime.datetime.strptime(new_value_str, '%Y-%m-%d').date()
                elif len(new_value_str) == 8 and new_value_str.isdigit():
                     converted_value = datetime.datetime.strptime(new_value_str, '%Y%m%d').date()
                elif not new_value_str.strip():
                    converted_value = None
                else:
                    raise ValueError("Invalid date format. Use YYYYMMDD or YYYY-MM-DD or leave blank.")
            elif field_obj.field_type == 'L':
                upper_val = new_value_str.upper()
                if upper_val in ('T', 'Y', 'TRUE', 'YES'):
                    converted_value = True
                elif upper_val in ('F', 'N', 'FALSE', 'NO'):
                    converted_value = False
                elif not new_value_str.strip():
                    converted_value = None
                else:
                    raise ValueError("Invalid logical value. Use T/F or Y/N or TRUE/FALSE or leave blank.")
            elif field_obj.field_type == 'M':
                 converted_value = str(new_value_str)
                 print(f"Warning: Setting Memo field '{field_obj.field_name}' directly might not update .DBT file. Only updates pointer if applicable.")
            else:
                converted_value = str(new_value_str)

            current_area._parsed_record_data[field_obj.field_name] = converted_value
            print(f"Field '{field_obj.field_name}' set to '{converted_value}' in memory.")
            print("Remember to 'WRITE' to save changes to the file.")

        except ValueError as e:
            print(f"Error converting '{new_value_str}' for field '{field_obj.field_name}' (Type: {field_obj.field_type}): {e}")
        except Exception as e:
            print(f"An unexpected error occurred while setting field: {e}")

    def do_write(self, *args):
        """
        Writes the current record's loaded data back to the file.
        Usage: WRITE
        (Implicitly writes to the current record number in the active area.)
        """
        current_area = self.current_area
        if not current_area:
            print("No active area or file open to write.")
            return

        if not current_area._parsed_record_data:
            print("No record data loaded to write. Use GOTO first.")
            return

        if current_area.write_rec():
            print(f"Record {current_area.current_rec_num} written.")
        else:
            print(f"Failed to write record {current_area.current_rec_num}.")

    def do_add(self, *args):
        """
        Adds a new, blank record to the active DBF file.
        Usage: ADD
        The new record becomes the current record.
        """
        current_area = self.current_area
        if not current_area:
            print("No active area or file open to add a record.")
            return

        if current_area.add_rec():
            print(f"New record added. Current record is now {current_area.current_rec_num}.")
            self.do_display()
        else:
            print("Failed to add new record.")

    def do_delete(self, *args):
        """
        Toggles the deleted status of the current record (marks/unmarks as deleted).
        Usage: DELETE
        """
        current_area = self.current_area
        if not current_area:
            print("No active area or record loaded to delete.")
            return

        if current_area.del_rec():
            status_str = "DELETED" if current_area.is_deleted else "NOT DELETED"
            print(f"Record {current_area.current_rec_num} marked as {status_str}.")
        else:
            print(f"Failed to toggle delete status for record {current_area.current_rec_num}.")

    def do_undelete(self, *args):
        """
        Alias for DELETE, toggles the deleted status of the current record.
        Usage: UNDELETE
        """
        self.do_delete(*args)

    def do_create(self, filename: str, area_id_str: str = None):
        """
        Creates a new DBF file with user-defined fields.
        Usage: CREATE <filename> [area_id]
        You will be prompted to define fields (Name, Type, Length, Decimals).
        Type 'done' when finished defining fields.
        """
        target_area_id = self._active_area_id
        if area_id_str:
            try:
                target_area_id = int(area_id_str)
            except ValueError:
                print(f"Error: Invalid area ID '{area_id_str}'. Must be a number.")
                return

        if not self.select_area(target_area_id):
            return

        current_area = self._areas.get(target_area_id)
        if not current_area: return

        field_defs = []
        print("\n--- Define Fields (Type 'done' for Field Name when finished) ---")
        while True:
            name = input("  Field Name (1-10 chars, DONE to finish): ").strip().upper()
            if name == 'DONE':
                break
            if not name or len(name) > 10:
                print("    Invalid name. Must be 1-10 characters.")
                continue

            type_ = input("  Field Type (C, N, D, L, M): ").strip().upper()
            if type_ not in ['C', 'N', 'D', 'L', 'M']:
                print("    Invalid type. Use C, N, D, L, or M.")
                continue

            length_ = input("  Field Length: ").strip()
            try:
                length = int(length_)
                if not (1 <= length <= 254):
                     print("    Length must be between 1 and 254.")
                     continue
            except ValueError:
                print("    Invalid length. Must be a number.")
                continue

            dec_ = 0
            if type_ == 'N':
                dec_str = input("  Decimal Places (for Numeric): ").strip()
                try:
                    dec_ = int(dec_str)
                    if not (0 <= dec_ <= length - 2):
                         print(f"    Decimal places must be between 0 and {length - 2} for numeric fields.")
                         continue
                except ValueError:
                    print("    Invalid decimal places. Must be a number.")
                    continue

            field_defs.append({
                'name': name,
                'type': type_,
                'length': length,
                'dec': dec_
            })
            print(f"  Field '{name}' added. ({len(field_defs)} fields defined)")

        if not field_defs:
            print("No fields defined. File not created.")
            return

        print(f"\nAttempting to create DBF file '{filename}' with {len(field_defs)} fields...")
        if current_area.create_dbf(filename, field_defs):
            print(f"Successfully created '{filename}'.")
        else:
            print(f"Failed to create '{filename}'.")

    def do_fields(self, *args):
        """
        Displays a list of all fields in the currently active DBF file.
        Usage: FIELDS
        """
        current_area = self.current_area
        if not current_area or not current_area.file_ok or not current_area.fields:
            print("No active area or DBF file open to list fields.")
            return

        print(f"\n--- Fields for Area {current_area.area_id} ({len(current_area.fields)} total) ---")
        print(f"{'Name':<15} {'Type':<6} {'Length':<8} {'Decimals':<10}")
        print(f"{'-'*15} {'-'*6} {'-'*8} {'-'*10}")
        for field in current_area.fields:
            dec_str = str(field.decimal_places) if field.field_type == 'N' else ''
            print(f"{field.field_name:<15} {field.field_type:<6} {field.field_length:<8} {dec_str:<10}")
        print("--------------------------------------------------")

    def do_quit(self, *args):
        """
        Exits the program.
        Usage: QUIT
        """
        print("Exiting XBASE.PY. Goodbye!")
        for area_id, area in self._areas.items():
            if area.file_ok:
                area.close_file()
        sys.exit(0)

# --- Global Database Instance ---
global_db_instance = DbfInstance()

# Function to mimic C's initbase() if needed externally
def initbase() -> int:
    """Initializes the global database instance (called by DbfInstance constructor)."""
    return 0
