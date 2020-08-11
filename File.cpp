#include "File.h"
#include "sha1.h"
#include <boost/lexical_cast.hpp>

uintmax_t File::getSize() const
{
	return this->size;
}

std::shared_ptr<File> File::makeFile(const std::string& name, uintmax_t size, time_t last_edit)
{
	std::shared_ptr<File> f = std::shared_ptr<File>(new File());
	f->name = name;
	f->size = size;
	f->last_edit = last_edit;
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

std::string File::getChecksum()
{
	return this->checksum;
}

void File::calculateChecksum()
{
	SHA1* sha1 = new SHA1();
	sha1->addBytes(this->name.c_str(), strlen(this->name.c_str()));
	std::string time_le = boost::lexical_cast<std::string>(this->last_edit).c_str();
	sha1->addBytes(time_le.c_str(), strlen(time_le.c_str()));

	this->checksum = sha1->getDigestToHexString();
}