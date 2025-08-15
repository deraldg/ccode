# xbase_cli.py

import textwrap
from dbf_core import global_db_instance, initbase
import sys

def print_banner():
    """Prints the initial HELP banner."""
    print(textwrap.dedent("""
    -----------------------------------------------------
    XBASE.PY - DBF File Utility (Python Port)
    Type 'HELP' for a list of commands.
    -----------------------------------------------------
    """).strip())

def run_cli():
    """Main loop for the command-line interface."""
    print_banner()
    while True:
        try:
            prompt_text = f"XBASE.PY (Area {global_db_instance._active_area_id})> "
            user_input = input(prompt_text).strip()

            if not user_input:
                continue

            parts = user_input.split(maxsplit=1)
            command = parts[0].lower()
            args = parts[1].split() if len(parts) > 1 else []

            if command in global_db_instance.commands:
                try:
                    global_db_instance.commands[command](*args)
                except SystemExit:
                    break
                except TypeError as e:
                    print(f"Error: Incorrect number/type of arguments for '{command}'. Details: {e}")
                    global_db_instance.do_help(command)
                except Exception as e:
                    print(f"An unexpected error occurred during command '{command}': {e}")
            else:
                print(f"Unknown command: '{command}'. Type 'HELP' for available commands.")
        except KeyboardInterrupt:
            print("\nCtrl+C detected. Type 'QUIT' to exit gracefully.")
        except Exception as e:
            print(f"An unhandled error occurred in the CLI: {e}")

# --- Entry Point ---
if __name__ == "__main__":
    initbase()
    run_cli()
