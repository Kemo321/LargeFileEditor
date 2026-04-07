import sys

def verify_operation(original_file, result_file, pos, insert_text):
    """
    Sprawdza czy insert na pozycji pos zadziałał poprawnie.
    Symuluje operację na poziomie bajtów i porównuje z plikiem wynikowym.
    """
    with open(original_file, 'r', encoding='utf-8') as f:
        original_data = f.read()

    # Symulacja operacji w Pythonie
    expected_data = original_data[:pos] + insert_text + original_data[pos:]

    try:
        with open(result_file, 'r', encoding='utf-8') as f:
            actual_data = f.read()

        if expected_data == actual_data:
            print("✅ SUKCES: Piece Table zadziałało poprawnie!")
            print(f"Rozmiar końcowy: {len(actual_data)} bajtów.")
        else:
            print("❌ BŁĄD: Dane wynikowe różnią się od oczekiwanych.")
            # Pokazuje fragment błędu
            for i, (e, a) in enumerate(zip(expected_data, actual_data)):
                if e != a:
                    print(f"Różnica na pozycji {i}: oczekiwano '{e}', otrzymano '{a}'")
                    break
    except FileNotFoundError:
        print(f"❌ BŁĄD: Nie znaleziono pliku wynikowego {result_file}")

if __name__ == "__main__":
    # Przykład użycia:
    # 1. Tworzysz plik przez gen_file.py
    # 2. Twój program C++ robi: table.insert(100, "TEST"); table.saveToFile("result.txt");
    # 3. Odpalasz ten skrypt:
    verify_operation("large_test_file.txt", "result.txt", 100, "TEST")
