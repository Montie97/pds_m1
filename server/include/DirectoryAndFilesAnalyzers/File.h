#include "DirectoryElement.h"

class File : public DirectoryElement
{
private:
	uintmax_t size;
	time_t last_edit;

public:
    int type() const;
    ~File() = default;
    void ls(int indent) const;
    uintmax_t getSize() const;
	static std::shared_ptr<File> makeFile(const std::string& name, uintmax_t size, time_t last_edit);
	std::string getChecksum();
	void calculateChecksum();
};