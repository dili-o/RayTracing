#include "File.hpp"

#include "Core/Memory.hpp"
#include "Core/Assert.hpp"
#include "Core/String.hpp"

#if defined(_WIN64)
#include <windows.h>
#include <shobjidl.h> 
#else
#define MAX_PATH 65536
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string.h>

namespace Helix {


    void file_open(cstring filename, cstring mode, FileHandle* file) {
#if defined(_WIN64)
        fopen_s(file, filename, mode);
#else
        * file = fopen(filename, mode);
#endif
    }

    void file_close(FileHandle file) {
        if (file)
            fclose(file);
    }

    sizet file_write(uint8_t* memory, u32 element_size, u32 count, FileHandle file) {
        return fwrite(memory, element_size, count, file);
    }

    static long file_get_size(FileHandle f) {
        long fileSizeSigned;

        fseek(f, 0, SEEK_END);
        fileSizeSigned = ftell(f);
        fseek(f, 0, SEEK_SET);

        return fileSizeSigned;
    }

#if defined(_WIN64)
    FileTime file_last_write_time(cstring filename) {
        FILETIME lastWriteTime = {};

        WIN32_FILE_ATTRIBUTE_DATA data;
        if (GetFileAttributesExA(filename, GetFileExInfoStandard, &data)) {
            lastWriteTime.dwHighDateTime = data.ftLastWriteTime.dwHighDateTime;
            lastWriteTime.dwLowDateTime = data.ftLastWriteTime.dwLowDateTime;
        }

        return lastWriteTime;
    }
#endif // _WIN64

    u32 file_resolve_to_full_path(cstring path, char* out_full_path, u32 max_size) {
#if defined(_WIN64)
        return GetFullPathNameA(path, max_size, out_full_path, nullptr);
#else
        return readlink(path, out_full_path, max_size);
#endif // _WIN64
    }

    void file_directory_from_path(char* path) {
        char* last_point = strrchr(path, '.');
        char* last_separator = strrchr(path, '/');
        if (last_separator != nullptr && last_point > last_separator) {
            *(last_separator + 1) = 0;
        }
        else {
            // Try searching backslash
            last_separator = strrchr(path, '\\');
            if (last_separator != nullptr && last_point > last_separator) {
                *(last_separator + 1) = 0;
            }
            else {
                // Wrong input!
                HASSERT_MSGS(false, "Malformed path {}!", path);
            }

        }
    }

    void file_name_from_path(char* path) {
        char* last_separator = strrchr(path, '/');
        if (last_separator == nullptr) {
            last_separator = strrchr(path, '\\');
        }

        if (last_separator != nullptr) {
            sizet name_length = strlen(last_separator + 1);

            memcpy(path, last_separator + 1, name_length);
            path[name_length] = 0;
        }
    }

    char* file_extension_from_path(char* path) {
        char* last_separator = strrchr(path, '.');

        return last_separator + 1;
    }

    bool file_exists(cstring path) {
#if defined(_WIN64)
        WIN32_FILE_ATTRIBUTE_DATA unused;
        return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
#else
        int result = access(path, F_OK);
        return (result == 0);
#endif // _WIN64
    }

    bool file_delete(cstring path) {
#if defined(_WIN64)
        int result = remove(path);
        return result != 0;
#else
        int result = remove(path);
        return (result == 0);
#endif
    }

    // https://stackoverflow.com/questions/68601080/how-do-you-open-a-file-explorer-dialogue-in-c
    bool file_open_dialog(char*& file_path, char*& filename){
        std::string sSelectedFile;
        std::string sFilePath;
        //  CREATE FILE OBJECT INSTANCE
        HRESULT f_SysHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        if (FAILED(f_SysHr))
            return FALSE;

        // CREATE FileOpenDialog OBJECT
        IFileOpenDialog* f_FileSystem;
        f_SysHr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&f_FileSystem));
        if (FAILED(f_SysHr)) {
            CoUninitialize();
            return FALSE;
        }

        //  SHOW OPEN FILE DIALOG WINDOW
        f_SysHr = f_FileSystem->Show(NULL);
        if (FAILED(f_SysHr)) {
            f_FileSystem->Release();
            CoUninitialize();
            return FALSE;
        }

        //  RETRIEVE FILE NAME FROM THE SELECTED ITEM
        IShellItem* f_Files;
        f_SysHr = f_FileSystem->GetResult(&f_Files);
        if (FAILED(f_SysHr)) {
            f_FileSystem->Release();
            CoUninitialize();
            return FALSE;
        }

        //  STORE AND CONVERT THE FILE NAME
        PWSTR f_Path;
        f_SysHr = f_Files->GetDisplayName(SIGDN_FILESYSPATH, &f_Path);
        if (FAILED(f_SysHr)) {
            f_Files->Release();
            f_FileSystem->Release();
            CoUninitialize();
            return FALSE;
        }

        //  FORMAT AND STORE THE FILE PATH
        std::wstring path(f_Path);


        std::string c(path.begin(), path.end());
        

        sFilePath = c;

        //  FORMAT STRING FOR EXECUTABLE NAME
        const size_t slash = sFilePath.find_last_of("/\\");
        sSelectedFile = sFilePath.substr(slash + 1);

        if (slash != std::string::npos) {
            sFilePath = sFilePath.substr(0, slash + 1);  // Keep everything up to the last slash
        }

        file_path = new char[sFilePath.size() + 1]; // +1 for the null terminator
        std::strcpy(file_path, sFilePath.c_str());

        filename = new char[sSelectedFile.size() + 1]; // +1 for the null terminator
        std::strcpy(filename, sSelectedFile.c_str());


        //  SUCCESS, CLEAN UP
        CoTaskMemFree(f_Path);
        f_Files->Release();
        f_FileSystem->Release();
        CoUninitialize();
        return TRUE;

        return false;
    }


    bool directory_exists(cstring path) {
#if defined(_WIN64)
        WIN32_FILE_ATTRIBUTE_DATA unused;
        return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
#else
        int result = access(path, F_OK);
        return (result == 0);
#endif // _WIN64
    }

    bool directory_create(cstring path) {
#if defined(_WIN64)
        int result = CreateDirectoryA(path, NULL);
        return result != 0;
#else
        int result = mkdir(path, S_IRWXU | S_IRWXG);
        return (result == 0);
#endif // _WIN64
    }

    bool directory_delete(cstring path) {
#if defined(_WIN64)
        int result = RemoveDirectoryA(path);
        return result != 0;
#else
        int result = rmdir(path);
        return (result == 0);
#endif // _WIN64
    }

    void directory_current(Directory* directory) {
#if defined(_WIN64)
        DWORD written_chars = GetCurrentDirectoryA(k_max_path, directory->path);
        directory->path[written_chars] = 0;
#else
        getcwd(directory->path, k_max_path);
#endif // _WIN64
    }

    void directory_change(cstring path) {
#if defined(_WIN64)
        if (!SetCurrentDirectoryA(path)) {
            HERROR("Cannot change current directory to {}", path);
        }
#else
        if (chdir(path) != 0) {
            HERROR("Cannot change current directory to {}", path);
        }
#endif // _WIN64
    }

    //
    static bool string_ends_with_char(cstring s, char c) {
        cstring last_entry = strrchr(s, c);
        const sizet index = last_entry - s;
        return index == (strlen(s) - 1);
    }

    void file_open_directory(cstring path, Directory* out_directory) {

        // Open file trying to conver to full path instead of relative.
        // If an error occurs, just copy the name.
        if (file_resolve_to_full_path(path, out_directory->path, MAX_PATH) == 0) {
            strcpy(out_directory->path, path);
        }

        // Add '\\' if missing
        if (!string_ends_with_char(path, '\\')) {
            strcat(out_directory->path, "\\");
        }

        if (!string_ends_with_char(out_directory->path, '*')) {
            strcat(out_directory->path, "*");
        }

#if defined(_WIN64)
        out_directory->os_handle = nullptr;

        WIN32_FIND_DATAA find_data;
        HANDLE found_handle;
        if ((found_handle = FindFirstFileA(out_directory->path, &find_data)) != INVALID_HANDLE_VALUE) {
            out_directory->os_handle = found_handle;
        }
        else {
            HERROR("Could not open directory {}", out_directory->path);
        }
#else
        HASSERT_MSG(false, "Not implemented");
#endif
    }

    void file_close_directory(Directory* directory) {
#if defined(_WIN64)
        if (directory->os_handle) {
            FindClose(directory->os_handle);
        }
#else
        RASSERTM(false, "Not implemented");
#endif
    }

    void file_parent_directory(Directory* directory) {

        Directory new_directory;

        cstring last_directory_separator = strrchr(directory->path, '\\');
        sizet index = last_directory_separator - directory->path;

        if (index > 0) {

            strncpy(new_directory.path, directory->path, index);
            new_directory.path[index] = 0;

            last_directory_separator = strrchr(new_directory.path, '\\');
            sizet second_index = last_directory_separator - new_directory.path;

            if (last_directory_separator) {
                new_directory.path[second_index] = 0;
            }
            else {
                new_directory.path[index] = 0;
            }

            file_open_directory(new_directory.path, &new_directory);

#if defined(_WIN64)
            // Update directory
            if (new_directory.os_handle) {
                *directory = new_directory;
            }
#else
            RASSERTM(false, "Not implemented");
#endif
        }
    }

    void file_sub_directory(Directory* directory, cstring sub_directory_name) {

        // Remove the last '*' from the path. It will be re-added by the file_open.
        if (string_ends_with_char(directory->path, '*')) {
            directory->path[strlen(directory->path) - 1] = 0;
        }

        strcat(directory->path, sub_directory_name);
        file_open_directory(directory->path, directory);
    }

    void file_find_files_in_path(cstring file_pattern, StringArray& files) {

        files.clear();

#if defined(_WIN64)
        WIN32_FIND_DATAA find_data;
        HANDLE hFind;
        if ((hFind = FindFirstFileA(file_pattern, &find_data)) != INVALID_HANDLE_VALUE) {
            do {

                files.intern(find_data.cFileName);

            } while (FindNextFileA(hFind, &find_data) != 0);
            FindClose(hFind);
        }
        else {
            HERROR("Cannot find file {}", file_pattern);
        }
#else
        RASSERTM(false, "Not implemented");
        // TODO(marco): opendir, readdir
#endif
    }

    void file_find_files_in_path(cstring extension, cstring search_pattern, StringArray& files, StringArray& directories) {

        files.clear();
        directories.clear();

#if defined(_WIN64)
        WIN32_FIND_DATAA find_data;
        HANDLE hFind;
        if ((hFind = FindFirstFileA(search_pattern, &find_data)) != INVALID_HANDLE_VALUE) {
            do {
                if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                    directories.intern(find_data.cFileName);
                }
                else {
                    // If filename contains the extension, add it
                    if (strstr(find_data.cFileName, extension)) {
                        files.intern(find_data.cFileName);
                    }
                }

            } while (FindNextFileA(hFind, &find_data) != 0);
            FindClose(hFind);
        }
        else {
            HERROR("Cannot find directory {}", search_pattern);
        }
#else
        RASSERTM(false, "Not implemented");
#endif
    }

    void environment_variable_get(cstring name, char* output, u32 output_size) {
#if defined(_WIN64)
        ExpandEnvironmentStringsA(name, output, output_size);
#else
        cstring real_output = getenv(name);
        strncpy(output, real_output, output_size);
#endif
    }

    char* file_read_binary(cstring filename, Allocator* allocator, sizet* size) {
        char* out_data = 0;

        FILE* file = fopen(filename, "rb");

        if (file) {

            // TODO: Use filesize or read result ?
            sizet filesize = file_get_size(file);

            out_data = (char*)halloca(filesize + 1, allocator);
            fread(out_data, filesize, 1, file);
            out_data[filesize] = 0;

            if (size)
                *size = filesize;

            fclose(file);
        }

        return out_data;
    }

    char* file_read_text(cstring filename, Allocator* allocator, sizet* size) {
        char* text = 0;

        FILE* file = fopen(filename, "r");

        if (file) {

            sizet filesize = file_get_size(file);
            text = (char*)halloca(filesize + 1, allocator);
            // Correct: use elementcount as filesize, bytes_read becomes the actual bytes read
            // AFTER the end of line conversion for Windows (it uses \r\n).
            sizet bytes_read = fread(text, 1, filesize, file);

            text[bytes_read] = 0;

            if (size)
                *size = filesize;

            fclose(file);
        }

        return text;
    }

    FileReadResult file_read_binary(cstring filename, Allocator* allocator) {
        FileReadResult result{ nullptr, 0 };

        FILE* file = fopen(filename, "rb");

        if (file) {

            // TODO: Use filesize or read result ?
            sizet filesize = file_get_size(file);

            result.data = (char*)halloca(filesize, allocator);
            fread(result.data, filesize, 1, file);

            result.size = filesize;

            fclose(file);
        }

        return result;
    }

    FileReadResult file_read_text(cstring filename, Allocator* allocator) {
        FileReadResult result{ nullptr, 0 };

        FILE* file = fopen(filename, "r");

        if (file) {

            sizet filesize = file_get_size(file);
            result.data = (char*)halloca(filesize + 1, allocator);
            // Correct: use elementcount as filesize, bytes_read becomes the actual bytes read
            // AFTER the end of line conversion for Windows (it uses \r\n).
            sizet bytes_read = fread(result.data, 1, filesize, file);

            result.data[bytes_read] = 0;

            result.size = bytes_read;

            fclose(file);
        }

        return result;
    }

    void file_write_binary(cstring filename, void* memory, sizet size) {
        FILE* file = fopen(filename, "wb");
        fwrite(memory, size, 1, file);
        fclose(file);
    }

    // Scoped file //////////////////////////////////////////////////////////////////
    ScopedFile::ScopedFile(cstring filename, cstring mode) {
        file_open(filename, mode, &file);
    }

    ScopedFile::~ScopedFile() {
        file_close(file);
    }
} // namespace Helix
