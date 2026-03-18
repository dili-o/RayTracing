//=================================================================================================
//
//  MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#if HLX_PLATFORM_WINDOWS
#include "Core/Assert.hpp"
#include "FileIO.h"
#include "PCH.h"
#include <windows.h>

namespace hlx {
// Returns true if a file exits
bool FileExists(std::string_view filePath) {
  DWORD fileAttr = GetFileAttributesA(filePath.data());
  if (fileAttr == INVALID_FILE_ATTRIBUTES)
    return false;

  return true;
}

// Returns true if a directory exists
bool DirectoryExists(std::string_view dirPath) {
  DWORD fileAttr = GetFileAttributesA(dirPath.data());
  return (fileAttr != INVALID_FILE_ATTRIBUTES &&
          (fileAttr & FILE_ATTRIBUTE_DIRECTORY));
}

void CreateNewDirectory(std::string_view dirPath) {
  Win32Call(CreateDirectoryA(dirPath.data(), nullptr));
}

// Returns the directory containing a file
std::string GetDirectoryFromFilePath(std::string_view filePath) {
  HASSERT(filePath.data());

  size_t idx = filePath.rfind('\\');
  if (idx != std::string::npos)
    return std::string(filePath.substr(0, idx + 1));

  idx = filePath.rfind('/');
  if (idx != std::string::npos)
    return std::string(filePath.substr(0, idx + 1));

  return std::string("");
}

// Returns the name of the file given the path (extension included)
std::string GetFileName(std::string_view filePath) {
  HASSERT(filePath.data());

  size_t idx = filePath.rfind('\\');
  if (idx != std::string::npos && idx < filePath.length() - 1)
    return std::string(filePath.substr(idx + 1));
  else
    return std::string(filePath);
}

// Returns the name of the file given the path, minus the extension
std::string GetFileNameWithoutExtension(std::string_view filePath) {
  std::string fileName = GetFileName(filePath);
  return GetFilePathWithoutExtension(fileName.c_str());
}

// Returns the given file path, minus the extension
std::string GetFilePathWithoutExtension(std::string_view filePath) {
  HASSERT(filePath.data());

  size_t idx = filePath.rfind('.');
  if (idx != std::string::npos)
    return std::string(filePath.substr(0, idx));
  else
    return std::string("");
}

// Returns the extension of the file path
std::string GetFileExtension(std::string_view filePath) {
  HASSERT(filePath.data());

  size_t idx = filePath.rfind('.');
  if (idx != std::string::npos)
    return std::string(filePath.substr(idx + 1, filePath.length() - idx - 1));
  else
    return std::string("");
}

// Gets the last written timestamp of the file
u64 GetFileTimestamp(std::string_view filePath) {
  HASSERT(filePath.data());

  WIN32_FILE_ATTRIBUTE_DATA attributes;
  Win32Call(GetFileAttributesExA(filePath.data(), GetFileExInfoStandard,
                                 &attributes));
  return attributes.ftLastWriteTime.dwLowDateTime |
         (u64(attributes.ftLastWriteTime.dwHighDateTime) << 32);
}

// Returns the contents of a file as a string
std::string ReadFileAsString(std::string_view filePath) {
  File file(filePath, File::OpenRead);
  u64 fileSize = file.Size();

  std::string fileContents;
  fileContents.resize(size_t(fileSize), 0);
  file.Read(fileSize, &fileContents[0]);

  return fileContents;
}

// Writes the contents of a string to a file
void WriteStringAsFile(std::string_view filePath, const std::string &data) {
  File file(filePath, File::OpenWrite);
  file.Write(data.length(), data.c_str());
}

// == File
// ========================================================================================

File::File() : fileHandle(INVALID_HANDLE_VALUE), openMode(OpenRead) {}

File::File(std::string_view filePath, OpenMode openMode)
    : fileHandle(INVALID_HANDLE_VALUE), openMode(OpenRead) {
  Open(filePath, openMode);
}

File::~File() {
  Close();
  HASSERT(fileHandle == INVALID_HANDLE_VALUE);
}

void File::Open(std::string_view filePath, OpenMode openMode_) {
  HASSERT(fileHandle == INVALID_HANDLE_VALUE);
  openMode = openMode_;

  if (openMode == OpenRead) {
    HASSERT(FileExists(filePath));

    // Open the file
    fileHandle = CreateFileA(filePath.data(), GENERIC_READ, FILE_SHARE_READ,
                             NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
      Win32Call(false);
  } else {
    // If the exists, delete it
    if (FileExists(filePath))
      Win32Call(DeleteFileA(filePath.data()));

    // Create the file
    fileHandle = CreateFileA(filePath.data(), GENERIC_WRITE, 0, NULL,
                             CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (fileHandle == INVALID_HANDLE_VALUE)
      Win32Call(false);
  }
}

void File::Close() {
  if (fileHandle == INVALID_HANDLE_VALUE)
    return;

  // Close the file
  Win32Call(CloseHandle(fileHandle));

  fileHandle = INVALID_HANDLE_VALUE;
}

void File::Read(u64 size, void *data) const {
  HASSERT(fileHandle != INVALID_HANDLE_VALUE);
  HASSERT(openMode == OpenRead);

  DWORD bytesRead = 0;
  Win32Call(
      ReadFile(fileHandle, data, static_cast<DWORD>(size), &bytesRead, NULL));
}

void File::Write(u64 size, const void *data) const {
  HASSERT(fileHandle != INVALID_HANDLE_VALUE);
  HASSERT(openMode == OpenWrite);

  DWORD bytesWritten = 0;
  Win32Call(WriteFile(fileHandle, data, static_cast<DWORD>(size), &bytesWritten,
                      NULL));
}

u64 File::Size() const {
  HASSERT(fileHandle != INVALID_HANDLE_VALUE);

  LARGE_INTEGER fileSize;
  Win32Call(GetFileSizeEx(fileHandle, &fileSize));

  return fileSize.QuadPart;
}

} // namespace hlx
#endif
