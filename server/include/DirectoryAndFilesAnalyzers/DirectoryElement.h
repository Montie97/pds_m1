#include <string>
#include <map>
#include <memory>
#include <iostream>

class DirectoryElement
{
protected:
    std::string name;
	std::string checksum = "";

public:
    std::string getName() const { return this->name; }
    virtual int type() const = 0;
    virtual void ls(int indent) const = 0;
	virtual void setName(const std::string& new_name) = 0;
	virtual std::string getChecksum() = 0;
	virtual void calculateChecksum() = 0;
};