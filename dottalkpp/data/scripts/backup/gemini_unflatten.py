import os
import sys
import re

def unflatten_sources(input_file_path: str):
    """
    Parses a flattened source file and recreates the original directory
    structure and source files.

    Args:
        input_file_path: The path to the text file containing the flattened sources.
    """
    output_base_dir = "unflattened_output"
    
    try:
        with open(input_file_path, 'r', encoding='utf-8') as f_in:
            content = f_in.read()
    except FileNotFoundError:
        print(f"❌ Error: Input file not found at '{input_file_path}'")
        return
    except Exception as e:
        print(f"❌ Error reading input file: {e}")
        return

    # Regex to find the file delimiter and extract the path
    # Example: // --- File: C:\Users\deral\code\ccode\src\cli\cmd_append.cpp ---
    file_delimiter_pattern = re.compile(r'// --- File: (.*) ---')
    
    # Split the entire content by the delimiter
    # The first split part is usually header info before the first file, so we skip it.
    file_sections = file_delimiter_pattern.split(content)[1:]

    if not file_sections:
        print("⚠️ Warning: No file delimiters found. Could not extract any files.")
        return

    print(f"🚀 Starting to unflatten '{input_file_path}'...")
    print(f"Output will be saved to the '{output_base_dir}' directory.")

    # The list is now [path1, content1, path2, content2, ...]
    for i in range(0, len(file_sections), 2):
        file_path_str = file_sections[i].strip()
        file_content = file_sections[i+1]

        # Clean the content by removing the markers
        cleaned_content = re.sub(r'\', '', file_content).strip()

        # Sanitize the path to be relative and safe for the current OS
        # This removes the drive letter like 'C:'
        drive, path_no_drive = os.path.splitdrive(file_path_str)
        # Create a relative path
        relative_path = path_no_drive.lstrip('\\/')
        
        output_path = os.path.join(output_base_dir, relative_path)

        try:
            # Ensure the directory for the file exists
            output_dir = os.path.dirname(output_path)
            if output_dir:
                os.makedirs(output_dir, exist_ok=True)
            
            # Write the cleaned content to the new file
            with open(output_path, 'w', encoding='utf-8') as f_out:
                f_out.write(cleaned_content)
            
            print(f"  ✅ Created: {output_path}")

        except Exception as e:
            print(f"  ❌ Error processing file '{output_path}': {e}")
            continue

    print("\n🎉 Unflattening complete!")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python unflatten.py <path_to_flattened_file.txt>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    unflatten_sources(input_file)