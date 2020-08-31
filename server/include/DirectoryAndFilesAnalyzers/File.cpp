#include "File.h"
#include <boost/lexical_cast.hpp>

uintmax_t File::getSize() const
{
	return this->size;
}

std::shared_ptr<File> File::makeFile(const std::string& name, uintmax_t size, time_t last_edit, std::weak_ptr <DirectoryElement> parent)
{
	std::shared_ptr<File> f = std::shared_ptr<File>(new File());
	f->name = name;
	f->size = size;
	f->last_edit = last_edit;
	f->parent = parent;
	f->self = f;
	f->check_not_removed_flag = true;
	return f;
}

void File::ls(int indent) const
{
	for (int i = 0; i < indent; i++)
		std::cout << " ";
	std::cout << this->name << " (" << this->size << "B" << ") {" << this->checksum << "}" << std::endl;
}

int File::type() const
{
	return 1;
}

void File::setName(const std::string& new_name)
{
	this->name = new_name;
}

void File::setCheckNotRemovedFlag(bool b)
{
	this->check_not_removed_flag = b;
}

bool File::getCheckNotRemovedFlag()
{
	return this->check_not_removed_flag;
}

std::string File::getChecksum()
{
	return this->checksum;
}

void File::calculateChecksum()
{
	SHA1* sha1 = new SHA1();
	std::string size = boost::lexical_cast<std::string>(this->size);
	std::string time_le = boost::lexical_cast<std::string>(this->last_edit);
	sha1->addBytes(this->name.c_str(), strlen(this->name.c_str()));
	sha1->addBytes(size.c_str(), strlen(size.c_str()));
	sha1->addBytes(time_le.c_str(), strlen(time_le.c_str()));
	
	this->checksum = sha1->getDigestToHexString();
}

std::string File::getPathRec(std::shared_ptr<DirectoryElement> de)
{
	std::string path = "";
	if (de->getParent().lock() != nullptr) {
		path = getPathRec(de->getParent().lock());
		path += "/";
		path += de->getName();
	}
	else
		path = '.';
	return path;
}

std::string File::getPath() const
{
	std::string path;
	std::shared_ptr<DirectoryElement> de = this->self.lock();
	path = getPathRec(de);
	return path;
}

time_t File::getLastEdit() {
	return this->last_edit;
}

bool File::isRoot() {
	return false;
}