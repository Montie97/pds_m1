#pragma once

#include <string>
#include <map>
#include <memory>
#include <iostream>

class DirectoryElement
{
protected:
	std::string name;
	std::string checksum = "";
	std::weak_ptr<DirectoryElement> parent;

public:
	std::string getName() const { return this->name; }
	virtual int type() const = 0;
	virtual void ls(int indent) const = 0;
	virtual void setName(const std::string& new_name) = 0;
	std::string getChecksum() const { return this->checksum; }
	virtual void calculateChecksum() = 0;
	std::weak_ptr<DirectoryElement> getParent() const { return this->parent; }
	virtual std::string getPath() const = 0;
};