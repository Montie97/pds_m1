#pragma once

#include "DirectoryElement.h"
#include "sha1.h"
#include <boost/lexical_cast.hpp>

class File : public DirectoryElement
{
private:
	uintmax_t size;
	time_t last_edit;
	std::weak_ptr<File> self;

public:
	int type() const;
	~File() = default;
	void ls(int indent) const;
	uintmax_t getSize() const;
	static std::shared_ptr<File> makeFile(const std::string& name, uintmax_t size, time_t last_edit, std::weak_ptr<DirectoryElement> parent);
	void setName(const std::string& new_name);
	void calculateChecksum();
	std::string getPath() const;
	static std::string getPathRec(std::shared_ptr<DirectoryElement> de);
	time_t getLastEdit();
};