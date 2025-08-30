for path in all_cpp_and_hpp_files:
    print(f"@file: {path}")
    print(f"@type: {'header' if path.endswith('.hpp') else 'source'}")
    print("@code:")
    print(open(path).read())
    print("\n---\n")