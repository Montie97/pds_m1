#include <iostream>
#include <boost/filesystem.hpp>
#include "Directory.h"
#include "sha1.h"

void build_dir(std::shared_ptr<Directory> dir, boost::filesystem::path p)
{
	for (auto x : boost::filesystem::directory_iterator(p)){
		//Se l'elemento è una directory, il processo si ripete ricorsivamente
		if (boost::filesystem::is_directory(x.path())) {
			std::shared_ptr<Directory> new_dir = dir->addDirectory(x.path().filename().string());
			if (new_dir == nullptr)
				std::cout << "ECCEZIONE DIR" << std::endl;
			build_dir(new_dir, x.path());
		}
		else {
			std::shared_ptr<File> new_file = dir->addFile(x.path().filename().string(), boost::filesystem::file_size(x.path()), boost::filesystem::last_write_time(x.path()));
			if (new_file == nullptr)
				std::cout << "ECCEZIONE FILE" << std::endl;
		}
	}
}

std::shared_ptr<Directory> build_dir_wrap(boost::filesystem::path p)
{
	if (boost::filesystem::exists(p)) {
		if (boost::filesystem::is_directory(p)) {
			std::shared_ptr<Directory> root = std::make_shared<Directory>(Directory());
			build_dir(root, p);
			return root;
		}
		else
			std::cout << "Must specify a directory" << std::endl;
	}
	else
		std::cout << "The specified path does not exist" << std::endl;
	return std::shared_ptr<Directory>(nullptr);
}

bool compareChecksum(std::shared_ptr<DirectoryElement> old_dir, std::shared_ptr<DirectoryElement> new_dir){
	return old_dir->getChecksum() == new_dir->getChecksum();
}

void sendModifiedFile(std::shared_ptr<File> file)
{
	std::cout << "Modified file " << file->getName() << std::endl;
}

void removedElement(std::shared_ptr<DirectoryElement> de)
{
	std::cout << "Removed element " << de->getName() << std::endl;
}

void addedElement(std::shared_ptr<DirectoryElement> de)
{
	std::cout << "Added element " << de->getName() << std::endl;
}

bool checkRenamed(std::shared_ptr<DirectoryElement> de1, std::shared_ptr<DirectoryElement> de2)
{
	return true;
}

void compareOldNewDir(std::shared_ptr<Directory> old_dir, std::shared_ptr<Directory> new_dir)
{
	std::vector<std::shared_ptr<DirectoryElement>> removedOrRenamed;
	std::vector<std::shared_ptr<DirectoryElement>> addedOrRenamed;

	auto old_dir_children = old_dir->getChildren(); //Senza fare questi passaggio intermedi crasha...boh
	auto new_dir_children = new_dir->getChildren();
	//Controllo che i checksum delle due cartelle combacino
	if (old_dir->getChecksum() != new_dir->getChecksum()) {
		for (auto it_old = old_dir_children.begin(); it_old != old_dir_children.end(); ++it_old)
		{
			auto it_new = new_dir_children.find(it_old->first); //Cerco se il nome esiste ancora nella directory corrente
			if (it_new != new_dir_children.end()) {
				if (!compareChecksum(it_old->second, it_new->second)) { //controlla se checksums uguali
					if (it_old->second->type() == 0) { //Se è una directory => discesa ricorsiva
						std::shared_ptr<Directory> old_dir_child = std::dynamic_pointer_cast<Directory>(it_old->second);
						std::shared_ptr<Directory> new_dir_child = std::dynamic_pointer_cast<Directory>(it_new->second);
						compareOldNewDir(old_dir_child, new_dir_child);
					}
					else { //Se è un file => invia direttamente il file modificato
						std::shared_ptr<File> file_to_send = std::dynamic_pointer_cast<File>(it_new->second);
						sendModifiedFile(file_to_send);
					}
				}
			}
			else { // il file potrebbe essere stato rimosso o rinominato
				removedOrRenamed.emplace_back(it_old->second);
			}
		}
		for (auto it_new = new_dir_children.begin(); it_new != new_dir_children.end(); ++it_new)
		{
			if (old_dir_children.count(it_new->first) == 0) {
				addedOrRenamed.emplace_back(it_new->second);
			}
		}

		// controllo incrociato per rename
		size_t i = 0; //Inizializzando i e j nel for mi dava problemi con la keyword auto
		for (auto it_i = removedOrRenamed.begin(); i < removedOrRenamed.size(); i++) {
			size_t j = 0;
			for (auto it_j = addedOrRenamed.begin(); j < addedOrRenamed.size(); j++) {
				if (checkRenamed(removedOrRenamed[i], addedOrRenamed[j])) {
					removedOrRenamed.erase(it_i);
					addedOrRenamed.erase(it_j);
				}
				else {
					it_i++;
					it_j++;
				}
			}
		}

		for (int i = 0; i < removedOrRenamed.size(); i++) {
			removedElement(removedOrRenamed[i]);
		}
		for (int i = 0; i < addedOrRenamed.size(); i++) {
			addedElement(addedOrRenamed[i]);
		}
	}
}

int main()
{
	std::string folder_path = "../Prova";
	boost::filesystem::path p(folder_path);
	std::shared_ptr<Directory> image_root = build_dir_wrap(p); //root dell'immagine del client
	image_root->setName("root");
	image_root->calculateChecksum();
	//image_root->ls(4);
	while (true) {
		std::chrono::milliseconds timespan(1000);
		std::this_thread::sleep_for(timespan);
		std::cout << "checking..." << std::endl;
		std::shared_ptr<Directory> current_root = build_dir_wrap(p); //root corrente della directory
		current_root->setName("root");
		current_root->calculateChecksum();
		//current_root->ls(4);
		compareOldNewDir(image_root, current_root);
		current_root.reset();
	}
	/*#define BYTES ""
	SHA1* sha1 = new SHA1();
	sha1->addBytes(BYTES, strlen(BYTES));
	unsigned char* digest = sha1->getDigest();
	sha1->hexPrinter(digest, 20);
	delete sha1;
	free(digest);*/
	return 0;
}