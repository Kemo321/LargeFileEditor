import os

def generate_large_file(file_name, size_mb):
    """Generuje plik tekstowy o rozmiarze size_mb."""
    size_bytes = size_mb * 1024 * 1024
    pattern = "Tekst testowy dla PieceTable. Numer linii: "

    print(f"Generowanie pliku {file_name} ({size_mb} MB)...")

    with open(file_name, 'w', encoding='utf-8') as f:
        current_size = 0
        line_num = 0
        while current_size < size_bytes:
            line = f"{pattern}{line_num}\n"
            f.write(line)
            current_size += len(line.encode('utf-8'))
            line_num += 1

    print("Gotowe!")

if __name__ == "__main__":
    generate_large_file("large_test_file.txt", 100) # 100 MB
