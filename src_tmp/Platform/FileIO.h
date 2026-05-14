//=================================================================================================
//
//  MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

// #include "Core/Utility.h"

namespace hlx {

// Utility functions
bool FileExists(std::string_view filePath);
bool DirectoryExists(std::string_view dirPath);
void CreateNewDirectory(std::string_view dirPath);
std::string GetDirectoryFromFilePath(std::string_view filePath);
std::string GetFileName(std::string_view filePath);
std::string GetFileNameWithoutExtension(std::string_view filePath);
std::string GetFilePathWithoutExtension(std::string_view filePath);
std::string GetFileExtension(std::string_view filePath);
u64 GetFileTimestamp(std::string_view filePath);

std::string ReadFileAsString(std::string_view filePath);
void WriteStringAsFile(std::string_view filePath, std::string_view data);

class File {

public:
  enum OpenMode {
    OpenRead = 0,
    OpenWrite = 1,
  };

private:
  // HANDLE fileHandle;
  void *fileHandle;
  OpenMode openMode;

public:
  // Lifetime
  File();
  File(std::string_view filePath, OpenMode openMode);
  ~File();

  // Explicit Open and close
  void Open(std::string_view filePath, OpenMode openMode);
  void Close();

  // I/O
  void Read(u64 size, void *data) const;
  void Write(u64 size, const void *data) const;

  template <typename T> void Read(T &data) const;
  template <typename T> void Write(const T &data) const;

  // Accessors
  u64 Size() const;

  static bool OpenFileDialog(std::string &outFileName,
                             std::string &outFilePath);
};

// == File
// ========================================================================================

template <typename T> void File::Read(T &data) const { Read(sizeof(T), &data); }

template <typename T> void File::Write(const T &data) const {
  Write(sizeof(T), &data);
}

// Templated helper functions

// Reads a POD type from a file
template <typename T> void ReadFromFile(std::string_view fileName, T &val) {
  File file(fileName, File::OpenRead);
  file.Read(val);
}

// Writes a POD type to a file
template <typename T>
void WriteToFile(std::string_view fileName, const T &val) {
  File file(fileName, File::OpenWrite);
  file.Write(val);
}

} // namespace hlx
