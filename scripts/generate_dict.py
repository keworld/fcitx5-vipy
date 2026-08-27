#!/usr/bin/env python3
import sys
from pathlib import Path

def main():
    if len(sys.argv) < 3:
        print("Usage: generate_dict.py <input_dic> <output_gperf>", file=sys.stderr)
        sys.exit(1)

    dic_path = Path(sys.argv[1])
    gperf_path = Path(sys.argv[2])

    if not dic_path.exists():
        print(f"Error: Dictionary file '{dic_path}' not found.", file=sys.stderr)
        sys.exit(1)

    words = set()
    with open(dic_path, 'r', encoding='utf-8', errors='ignore') as f_in:
        for line in f_in:
            line = line.strip()
            # Bỏ qua dòng trống hoặc comment
            if not line or line.startswith('#'):
                continue
            # Xử lý định dạng hunspell (word/flags) hoặc danh sách từ thuần
            w = line.split('/')[0].strip().lower()
            if w:
                words.add(w)

    with open(gperf_path, 'w', encoding='utf-8') as f_out:
        f_out.write("%{\n#include <cstring>\n%}\n")
        f_out.write("struct SyllableWord { const char *name; };\n%%\n")
        for w in sorted(words):
            f_out.write(f"{w}\n")

if __name__ == "__main__":
    main()
