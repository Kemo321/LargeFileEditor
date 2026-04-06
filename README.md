# Large File Editor

A high-performance GUI text editor designed to handle files exceeding available RAM (1 GB+) efficiently. Developed as part of the ZPR (Advanced Object-Oriented Programming) course.

## 🚀 Key Features
* **Instant Opening:** Uses memory-mapped files (`QFile::map`) to handle gigabyte-sized files without delay.
* **Piece Table Implementation:** Core data structure that manages text modifications (insert/delete) without rewriting the original file.
* **Lazy Loading:** Dynamically fetches and renders only the visible portion of the text to ensure a smooth GUI experience.
* **Streaming Search:** Implements search algorithms (e.g., KMP) directly on the file stream to minimize memory footprint.

## 🛠 Tech Stack & Requirements
* **Platform:** Ubuntu 24.04 LTS (Target Environment)
* **Language:** C++17 (Strict adherence to ZPR coding standards)
* **GUI Framework:** Qt 6
* **Build System:** CMake
* **Unit Testing:** GoogleTest (integrated via Git Submodule)

## 🏗 Build Instructions

### 1. Install Dependencies
Ensure your Ubuntu 24.04 system has the necessary packages:
```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libgtest-dev