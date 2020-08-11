#include "File.h"
#include "sha1.h"
#include <exception>

class Directory : public DirectoryElement, public std::enable_shared_from_this<Directory>
{
private:
    std::weak_ptr<Directory> self;
    std::weak_ptr<Directory> parent;
    std::map<std::string, std::shared_ptr<DirectoryElement>> children;

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
    std::string getChecksum();
    void calculateChecksum();
    std::map<std::string, std::shared_ptr<DirectoryElement>> getChildren();
};