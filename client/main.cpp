#include <iostream>
#include <boost/filesystem.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include "Directory.h"

using boost::asio::ip::tcp;

enum CommunicationCodes { START_COMMUNICATION, END_COMMUNICATION, VERIFY_CHECKSUM, OK, NOT_OK, MISSING_ELEMENT, /*START_SEND_DIR, END_SEND_DIR,*/ MK_DIR, RMV_ELEMENT, RNM_ELEMENT, START_SEND_FILE, SENDING_FILE, END_SEND_FILE };

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
			root->setSelf(root);
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

void sendFile(std::shared_ptr<File> file, tcp::socket& socket)
{
	boost::system::error_code error;
	boost::array<char, 1024> buf;
	std::ifstream source_file(file->getPath(), std::ios_base::binary | std::ios_base::ate);
	if (!source_file) {
		std::cout << "failed to open: " << file->getPath() << std::endl;
		return; // throws exception?
	}

	std::string file_path = file->getPath();
	std::time_t last_edit = file->getLastEdit();
	size_t file_size = source_file.tellg();
	source_file.seekg(0);

	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	std::cout << file_path << "\n" << file_size << "\n" << last_edit << "\n\n";
	request_stream << START_SEND_FILE << "\n" << file_path << "\n" << file_size << "\n" << last_edit << "\n\n";
	boost::asio::write(socket, request);

	std::cout << "start sending file content.\n";
	try {
		while (true) {
			if (source_file.eof() == false) {
				source_file.read(buf.c_array(), (std::streamsize)buf.size());
				if (source_file.gcount() <= 0) {
					std::cout << "read file error" << std::endl;
					return; // throws exception?
				}

				request_stream << SENDING_FILE << "\n\n";
				boost::asio::write(socket, request);

				boost::asio::write(socket, boost::asio::buffer(buf.c_array(), source_file.gcount()), boost::asio::transfer_all(), error);
				if (error) {
					std::cout << "send error: " << error << std::endl;
					return; // throws exception?
				}
			}
			else {
				request_stream << END_SEND_FILE << "\n\n";
				boost::asio::write(socket, request);
				break;
			}
		}
	}
	catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}
}

void sendModifiedFile(std::shared_ptr<File> file, tcp::socket& socket){
	sendFile(file, socket);
}

void removedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket)
{
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	std::cout << "Rimosso: " << de->getPath() << "\n\n";
	request_stream << RMV_ELEMENT << "\n" << de->getPath() << "\n\n";
	boost::asio::write(socket, request); // gestire errori
}

void addedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket)
{
	boost::asio::streambuf request;
	std::ostream request_stream(&request);

	if (de->type() == 0) { // è directory
		std::cout << "Dir creata: " << de->getPath() << "\n\n";
		request_stream << MK_DIR << "\n" << de->getPath() << "\n\n";
		boost::asio::write(socket, request); // gestire errori
	}
	else {
		sendFile(std::dynamic_pointer_cast<File>(de), socket);
	}
}

bool checkRenamed(std::shared_ptr<Directory> dir1, std::shared_ptr<Directory> dir2)
{
	// se il numero di file all'interno delle dir è diverso, ritorna false
	if (dir1->getChildren().size() != dir2->getChildren().size()) {
		return false;
	}

	for (auto it = dir1->getChildren().begin(); it != dir1->getChildren().end(); ++it)
	{
		if (dir2->getChildren().count(it->first) > 0) { // il nome esiste in dir2
			if (!compareChecksum(it->second, dir2->getChildren()[it->first])) { // se checksum disuguali ritorna false
				return false;
			}
		}
		else { // se un nome non è presente in dir2, ritorna false
			return false;
		}
	}

	// tutti i file hanno la relativa controparte con lo stesso nome e lo stesso checksum in entrambe le dir
	return true;
}

void compareOldNewDir(std::shared_ptr<Directory> old_dir, std::shared_ptr<Directory> new_dir, tcp::socket& socket)
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
						compareOldNewDir(old_dir_child, new_dir_child, socket);
					}
					else { //Se è un file => invia direttamente il file modificato
						std::shared_ptr<File> file_to_send = std::dynamic_pointer_cast<File>(it_new->second);
						sendModifiedFile(file_to_send, socket);
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
				if (checkRenamed(std::dynamic_pointer_cast<Directory>(removedOrRenamed[i]), std::dynamic_pointer_cast<Directory>(addedOrRenamed[j]))) {
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
			removedElement(removedOrRenamed[i], socket);
		}
		for (int i = 0; i < addedOrRenamed.size(); i++) {
			addedElement(addedOrRenamed[i], socket);
		}
	}
}

int main()
{
	std::string folder_path = "../Prova"; //Il path della directory da monitorare andrà poi specificato in altro modo
	boost::filesystem::path p(folder_path);
	std::shared_ptr<Directory> image_root = build_dir_wrap(p); //root dell'immagine del client
	image_root->setName("root");
	image_root->calculateChecksum();
	//Connessione al server
	std::string server_ip_port = "127.0.0.1:1234"; //L'IP e la porta del server saranno poi da specificare in altro modo
	size_t pos = server_ip_port.find(':');
	if (pos == std::string::npos)
		return __LINE__;
	std::string server_port = server_ip_port.substr(pos + 1);
	std::string server_ip = server_ip_port.substr(0, pos);
	boost::asio::io_service io_service;
	tcp::resolver resolver(io_service);
	tcp::resolver::query query(server_ip, server_port);
	tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
	tcp::resolver::iterator end;
	tcp::socket socket(io_service);
	boost::system::error_code error = boost::asio::error::host_not_found;
	while (error && endpoint_iterator != end)
	{
		socket.close();
		socket.connect(*endpoint_iterator++, error);
	}
	if (error)
		return __LINE__;
	std::cout << "Connected to " << server_ip_port << std::endl;
	//Loop di controllo (deve essere possibile chiuderlo)
	while (true) {
		image_root->ls(4);
		std::chrono::milliseconds timespan(1000);
		std::this_thread::sleep_for(timespan);
		std::cout << "checking..." << std::endl;
		std::shared_ptr<Directory> current_root = build_dir_wrap(p); //root della directory corrente
		current_root->setName("root");
		current_root->calculateChecksum();
		//current_root->ls(4);
		compareOldNewDir(image_root, current_root, socket);
		image_root = std::move(current_root);
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