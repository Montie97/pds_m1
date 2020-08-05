#include "Directory.h"
#include "sha1/sha1.h"

std::shared_ptr<Directory> Directory::root = Directory::makeDirectory("", std::weak_ptr<Directory>()); //Inizializzazione di root

std::shared_ptr<Directory> Directory::makeDirectory(std::string name, std::weak_ptr <Directory> parent)
{
    std::shared_ptr<Directory> dir = std::shared_ptr<Directory>(new Directory());
    dir->name = name;
    dir->parent = parent;
    dir->self = dir;
    return dir;
}

std::shared_ptr<Directory> Directory::getRoot()
{
    if (Directory::root->name != "/")
        Directory::root = Directory::makeDirectory("/", std::weak_ptr<Directory>());
    return Directory::root;
}

std::shared_ptr<Directory> Directory::addDirectory(std::string name)
{
	std::shared_ptr<Directory> dir = shared_from_this();

	if (name.find("/") != std::string::npos) {
		std::string path = name.substr(0, name.rfind("/"));
		name = name.substr(name.rfind("/") + 1, name.length());
		dir = std::dynamic_pointer_cast<Directory>(searchDirEl(path));
		if (dir == nullptr)
			return std::shared_ptr<Directory>(nullptr);
	}

	if (name == ".." || name == ".")
		return std::shared_ptr<Directory>(nullptr);
	auto search = dir->children.find(name);
	if (search != dir->children.end())
		return std::shared_ptr<Directory>(nullptr);
	else {
		std::shared_ptr<Directory> new_dir = makeDirectory(name, dir->self);
		dir->children.insert({ name, new_dir });
		return new_dir;
	}
}

std::shared_ptr<File> Directory::addFile(std::string name, uintmax_t size, time_t last_edit)
{
	std::shared_ptr<Directory> dir = shared_from_this();

	if (name.find("/") != std::string::npos) {
		std::string path = name.substr(0, name.rfind("/"));
		name = name.substr(name.rfind("/") + 1, name.length());
		dir = std::dynamic_pointer_cast<Directory>(searchDirEl(path));
		if (dir == nullptr)
			return std::shared_ptr<File>(nullptr);
	}

	//std::cout << "gino: " << dir->getName() << std::endl;

    auto search = dir->children.find(name);
    if (search != dir->children.end())
        return std::shared_ptr<File>(nullptr);
    else {
        std::shared_ptr<File> f = File::makeFile(name, size, last_edit);
        dir->children.insert({ name, f });
        return f;
    }
}

std::shared_ptr<DirectoryElement> Directory::get(const std::string& name)
{
    if (name == "..")
        return this->parent.lock();
    if (name == ".")
        return this->self.lock();
    auto search = Directory::children.find(name);
    if (search != Directory::children.end())
        return search->second;
    else {
        std::cout << name << " not found" << std::endl;
        return std::shared_ptr<Directory>(nullptr);
    }
}

std::shared_ptr<DirectoryElement> Directory::searchDirEl(const std::string& _path)
{
	std::string path = _path;
	auto sp = std::make_shared<Directory>(*this);
	std::shared_ptr<Directory> curr_dir = sp;

	std::string delimiter = "/";
	std::string token;
	size_t pos = 0;

	while ((pos = path.find(delimiter)) != std::string::npos) {
		token = path.substr(0, pos);

		//std::cout << "gino " << token << std::endl;

		if (token == "..") {
			curr_dir = curr_dir->parent.lock();
			//std::cout << "gino4 " << curr_dir->getName() << std::endl;
		}
		else if (token == ".") {
			curr_dir = curr_dir->self.lock();
		}
		else {
			if (curr_dir->children.count(token) != 0) {    // Se ha trovato un match
				curr_dir = std::dynamic_pointer_cast<Directory>(curr_dir->children[token]);
				if (!curr_dir)
					std::cout << "ECCEZIONE, TROVATO FILE DI MEZZO" << std::endl;
			}
			else {
				return std::shared_ptr<Directory>(nullptr);
			}
		}

		path.erase(0, pos + delimiter.length());
	}

	//std::cout << "gino3 " << curr_dir->getName() << std::endl;
	return curr_dir;
}

int Directory::type() const {
    return 0;
}

std::shared_ptr<Directory> Directory::getDir(const std::string& name)
{
    std::shared_ptr<DirectoryElement> p = Directory::get(name);
    return std::dynamic_pointer_cast<Directory>(p);
}

std::shared_ptr<File> Directory::getFile(const std::string& name)
{
    std::shared_ptr<DirectoryElement> p = Directory::get(name);
    return std::dynamic_pointer_cast<File>(p);
}

void Directory::ls(int indent) const
{
    for (int i = 0; i < indent; i++)
        std::cout << " ";
    std::cout << "[+] " << this->name << " {" << this->checksum << "}" << std::endl;
    for (auto it = this->children.begin(); it != this->children.end(); ++it)
        it->second->ls(indent + 4);
}

bool Directory::remove(const std::string& nome)
{
    if (nome == ".." || nome == ".")
        return false;
    if (this->children.erase(nome) == 1)
        return true;
    return false;
}

std::string Directory::getChecksum()
{
	return this->checksum;
}

void Directory::calculateChecksum()
{
	SHA1* sha1 = new SHA1();
	sha1->addBytes(this->name.c_str(), strlen(this->name.c_str()));

	for (auto it = this->children.begin(); it != this->children.end(); ++it) {
		if (it->second->getChecksum() == "") {
			it->second->calculateChecksum();
		}

		sha1->addBytes(it->second->getChecksum().c_str(), strlen(it->second->getChecksum().c_str()));
	}

	this->checksum = sha1->getDigestToHexString();
}