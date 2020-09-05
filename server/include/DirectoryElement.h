#include <string>
#include <map>
#include <memory>
#include <iostream>
#include "sha1.h"

class DirectoryElement
{
protected:
	std::string name;
	std::string checksum = "";
	std::weak_ptr<DirectoryElement> parent;
	bool check_not_removed_flag;

public:
	std::string getName() const { return this->name; }
	virtual int type() const = 0;
	virtual void ls(int indent) const = 0;
	virtual void setName(const std::string& new_name) = 0;
	virtual void setCheckNotRemovedFlag(bool b) = 0;
	virtual bool getCheckNotRemovedFlag() = 0;
	virtual std::string getChecksum() = 0;
	virtual void calculateChecksum() = 0;
	std::weak_ptr<DirectoryElement> getParent() const { return this->parent; }
	virtual std::string getPath() const = 0;
	virtual bool isRoot() = 0;
};