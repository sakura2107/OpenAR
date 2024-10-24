#pragma once
#include <windows.h>
#include <shobjidl.h> // For IFileDialog
#include <string>

[[nodiscard]] inline std::string ShowFolderSelectionDialog() {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        return {};
    }

    std::string selectedPath;
    IFileDialog* pfd = nullptr;

    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr)) {
        CoUninitialize();
        return {};
    }
    DWORD dwFlags;
    hr = pfd->GetOptions(&dwFlags);
    if (SUCCEEDED(hr)) {
        hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS); // Allow folder picking
    }

    if (SUCCEEDED(hr)) {
        COMDLG_FILTERSPEC rgSpec[] = { { L"All Files", L"*.*" } };
        pfd->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
        pfd->SetFileTypeIndex(0);
        hr = pfd->Show(NULL);
    }
    if (SUCCEEDED(hr)) {
        IShellItem* pShellItem = nullptr;
        hr = pfd->GetResult(&pShellItem);
        if (SUCCEEDED(hr)) {
            LPWSTR file_path = nullptr;
            hr = pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
            if (SUCCEEDED(hr)) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, file_path, -1, NULL, 0, NULL, NULL);
                selectedPath.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, file_path, -1, &selectedPath[0], size_needed, NULL, NULL);
                CoTaskMemFree(file_path);
            }
            pShellItem->Release();
        }
    }
    pfd->Release();
    CoUninitialize();
    return selectedPath;
}

[[nodiscard]] inline std::string ShowJsonFileSelectionDialog() {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        return {};
    }

    std::string selectedPath;
    IFileDialog* pfd = nullptr;

    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr)) {
        CoUninitialize();
        return {};
    }

    DWORD dwFlags;
    hr = pfd->GetOptions(&dwFlags);
    if (SUCCEEDED(hr)) {
        hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
    }

    if (SUCCEEDED(hr)) {
        COMDLG_FILTERSPEC rgSpec[] = { { L"JSON Files", L"*.json" } };
        pfd->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
        pfd->SetFileTypeIndex(0);
        hr = pfd->Show(NULL);
    }

    if (SUCCEEDED(hr)) {
        IShellItem* pShellItem = nullptr;
        hr = pfd->GetResult(&pShellItem);
        if (SUCCEEDED(hr)) {
            LPWSTR file_path = nullptr;
            hr = pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
            if (SUCCEEDED(hr)) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, file_path, -1, NULL, 0, NULL, NULL);
                selectedPath.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, file_path, -1, &selectedPath[0], size_needed, NULL, NULL);
                CoTaskMemFree(file_path);
            }
            pShellItem->Release();
        }
    }

    pfd->Release();
    CoUninitialize();
    return selectedPath;
}

[[nodiscard]] inline std::string ShowImageFileSelectionDialog() {
    HRESULT hr = CoInitialize(NULL);
    if (FAILED(hr)) {
        return {};
    }

    std::string selectedPath;
    IFileDialog* pfd = nullptr;

    hr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pfd));
    if (FAILED(hr)) {
        CoUninitialize();
        return {};
    }

    DWORD dwFlags;
    hr = pfd->GetOptions(&dwFlags);
    if (SUCCEEDED(hr)) {
        hr = pfd->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
    }

    if (SUCCEEDED(hr)) {
        COMDLG_FILTERSPEC rgSpec[] = { { L"JPG Files", L"*.jpg" },
                                       { L"PNG Files", L"*.png" } };
        pfd->SetFileTypes(ARRAYSIZE(rgSpec), rgSpec);
        pfd->SetFileTypeIndex(0);
        hr = pfd->Show(NULL);
    }

    if (SUCCEEDED(hr)) {
        IShellItem* pShellItem = nullptr;
        hr = pfd->GetResult(&pShellItem);
        if (SUCCEEDED(hr)) {
            LPWSTR file_path = nullptr;
            hr = pShellItem->GetDisplayName(SIGDN_FILESYSPATH, &file_path);
            if (SUCCEEDED(hr)) {
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, file_path, -1, NULL, 0, NULL, NULL);
                selectedPath.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, file_path, -1, &selectedPath[0], size_needed, NULL, NULL);
                CoTaskMemFree(file_path);
            }
            pShellItem->Release();
        }
    }

    pfd->Release();
    CoUninitialize();
    return selectedPath;
}