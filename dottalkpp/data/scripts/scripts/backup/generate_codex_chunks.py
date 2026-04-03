import os

# Settings
root_dir = "."  # Start from current directory
extensions = [".cpp", ".hpp"]
chunk_size_limit = 100 * 1024  # 100 KB per chunk
output_prefix = "copilot_flatfile_"

# Internal buffers
chunks = []
current_chunk = ""
current_size = 0
chunk_index = 1

def flush_chunk():
    global current_chunk, current_size, chunk_index
    if current_chunk:
        filename = f"{output_prefix}{chunk_index}.txt"
        with open(filename, "w", encoding="utf-8") as f:
            f.write(current_chunk)
        print(f"✅ Created {filename} ({current_size} bytes)")
        chunks.append(filename)
        chunk_index += 1
        current_chunk = ""
        current_size = 0

# Walk and process files
for subdir, _, files in os.walk(root_dir):
    for file in files:
        if any(file.endswith(ext) for ext in extensions):
            full_path = os.path.join(subdir, file)
            try:
                with open(full_path, "r", encoding="utf-8") as f:
                    content = f.read()
                    block = f"@file: {full_path}\n@type: {'header' if file.endswith('.hpp') else 'source'}\n@code:\n{content}\n---\n\n"
                    block_size = len(block.encode("utf-8"))
                    if current_size + block_size > chunk_size_limit:
                        flush_chunk()
                    current_chunk += block
                    current_size += block_size
            except Exception as e:
                print(f"⚠️ Error reading {full_path}: {e}")

# Final flush
flush_chunk()

print(f"\n🎯 Total chunks created: {len(chunks)}")