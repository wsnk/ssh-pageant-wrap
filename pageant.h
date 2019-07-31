#pragma once
#include "common.h"
#include <windef.h>
#include <winbase.h>


// RAII pattern for anonymous file mapping
class FileMapping {
public:
    FileMapping(const FileMapping&) = delete;
    FileMapping& operator =(const FileMapping&) = delete;

public:
    FileMapping() = default;
    FileMapping(const std::string& mappingName, size_t maxSize, SECURITY_ATTRIBUTES* sa = nullptr);
    ~FileMapping();

    FileMapping(FileMapping&&);
    FileMapping& operator =(FileMapping&&);

    const std::string& GetName() const { return Name; }
    void* GetView() const { return View; }

private:
    std::string Name;
    HANDLE Handle = nullptr;
    void* View = nullptr;
};


// For now only one instance can work correctly
class Pageant {
public:
    Pageant(const Pageant&) = delete;
    Pageant& operator =(const Pageant&) = delete;

    static void MakeError(Buffer& msg);

public:
    Pageant();
    ~Pageant();

    void Query(Buffer& msg) const;
    void TryQuery(Buffer& msg) const noexcept;

private:
    mutable HWND Hwnd;
    FileMapping Mapping;
};

