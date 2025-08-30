import os

# Root directory of your project
root_dir = "."

# Output file
output_file = "copilot_flatfile.txt"

# File extensions to include
extensions = [".cpp", ".hpp"]

with open(output_file, "w", encoding="utf-8") as out:
    for subdir, _, files in os.walk(root_dir):
        for file in files:
            if any(file.endswith(ext) for ext in extensions):
                full_path = os.path.join(subdir, file)
                try:
                    with open(full_path, "r", encoding="utf-8") as f:
                        content = f.read()
                        out.write(f"@file: {full_path}\n")
                        out.write(f"@type: {'header' if file.endswith('.hpp') else 'source'}\n")
                        out.write("@code:\n")
                        out.write(content)
                        out.write("\n---\n\n")
                except Exception as e:
                    print(f"Error reading {full_path}: {e}")