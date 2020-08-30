#pragma once

#include "DirectoryElement.h"
#include "File.h"
#include <exception>

class Directory : public DirectoryElement, public std::enable_shared_from_this<Directory>
{
private:
    std::weak_ptr<Directory> self;
    std::map<std::string, std::shared_ptr<DirectoryElement>> children;
    bool is_root;

public:

    int type() const;
    Directory() = default;
    ~Directory() = default;
    void ls(int indent) const;
    bool remove(std::string name);
    bool rename(std::string old_name, std::string new_name);
    std::shared_ptr<File> getFile(const std::string& name);
    std::shared_ptr<Directory> getDir(const std::string& name);
    std::shared_ptr<DirectoryElement> searchDirEl(const std::string& path);
    std::shared_ptr<DirectoryElement> get(const std::string& name);
    std::shared_ptr<Directory> addDirectory(std::string name);
    std::shared_ptr<File> addFile(std::string name, uintmax_t size, time_t last_edit);
    static std::shared_ptr<Directory> makeDirectory(std::string name, std::weak_ptr<Directory> parent);
    void setName(const std::string& new_name);
    void calculateChecksum();
    std::map<std::string, std::shared_ptr<DirectoryElement>> getChildren();
    void setSelf(std::weak_ptr<Directory> self);
    std::string getPath() const;
    static std::string getPathRec(std::shared_ptr<DirectoryElement> de);
    bool isRoot();
    void setIsRoot(bool is_root);
};