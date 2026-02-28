import sys
from pathlib import Path

from PIL import Image, UnidentifiedImageError


def is_valid_jpeg(file_path):
    """
    Checks if a file is a valid JPEG by reading its headers.
    Ignores file extensions entirely.
    """
    try:
        # Image.open loads the file header but not the raster data
        with Image.open(file_path) as img:
            img.verify()  # Verifies that it is a structurally valid image

            # Check if the internal format is actually JPEG
            if img.format == 'JPEG':
                return True
    except (UnidentifiedImageError, IsADirectoryError, PermissionError):
        # The file is not an image, or cannot be read
        return False
    except Exception as e:
        # Catches other potential file reading errors (e.g., corrupted data)
        return False

    return False

def find_jpegs_recursively(folder_path):
    """
    Recursively scans a folder and separates valid JPEGs from other files.
    """
    folder = Path(folder_path)

    if not folder.is_dir():
        print(f"Error: The path '{folder_path}' is not a valid directory.")
        return

    print(f"Scanning '{folder_path}' recursively. This might take a moment depending on the number of files...\n")

    valid_jpegs = []
    other_files = []

    # rglob('*') iterates through all files and folders recursively
    for file_path in folder.rglob('*'):
        if file_path.is_file():
            if is_valid_jpeg(file_path):
                valid_jpegs.append(file_path)
            else:
                other_files.append(file_path)

    # --- Print Results ---
    print(f"--- Scan Complete ---")
    print(f"Found {len(valid_jpegs)} valid JPEG(s):")
    for jpeg in valid_jpegs:
        print(f"  [VALID] {jpeg}")

    print(f"\nFound {len(other_files)} non-JPEG or corrupted file(s).")
    # Uncomment the next two lines if you want to print the non-JPEG files too
    # for non_jpeg in other_files:
    #     print(f"  [INVALID/OTHER] {non_jpeg}")

if __name__ == "__main__":
    # You can change this path to whatever folder you want to scan
    target_folder = "./"

    # Alternatively, take the folder path from command line arguments
    if len(sys.argv) > 1:
        target_folder = sys.argv[1]

    find_jpegs_recursively(target_folder)
