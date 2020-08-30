#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include "Directory.h"

#define MULTITHREADING

#ifdef MULTITHREADING
#include <thread>
#endif

using boost::asio::ip::tcp;

enum CommunicationCodes { START_COMMUNICATION, END_COMMUNICATION, VERIFY_CHECKSUM, OK, NOT_OK, MISSING_ELEMENT, MK_DIR, RMV_ELEMENT, RNM_ELEMENT, START_SEND_FILE, SENDING_FILE, END_SEND_FILE, START_SYNC, END_SYNC, VERSION_MISMATCH };

void addedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket, std::string directory_path); //Serve il prototipo perché c'è una ricorsione "indiretta" in sendDir" (sendDir chiama addedElement e addedElement chiama sendDir)

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

std::shared_ptr<Directory> build_dir_wrap(boost::filesystem::path p, std::string root_name)
{
	if (boost::filesystem::exists(p)) {
		if (boost::filesystem::is_directory(p)) {
			std::shared_ptr<Directory> root = std::make_shared<Directory>(Directory());
			root->setSelf(root);
			build_dir(root, p);
			root->setName(root_name);
			root->setIsRoot(true);
			root->setSelf(root);
			root->calculateChecksum();
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

void sendFile(std::shared_ptr<File> file, tcp::socket& socket, std::string directory_path)
{
	boost::system::error_code error;
	boost::array<char, 1024> buf;
	std::ifstream source_file(directory_path + '/' + file->getPath(), std::ios_base::binary | std::ios_base::ate);
	if (!source_file.is_open()) {
		std::cout << "Failed to open: " << file->getPath() << std::endl;
		return; // throws exception?
	}

	std::string file_path = file->getPath();
	std::time_t last_edit = file->getLastEdit();
	size_t file_size = source_file.tellg();
	source_file.seekg(0);

	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << START_SEND_FILE << "\n" << file_path << "\n" << file_size << "\n" << last_edit << "\n\n";
	boost::asio::write(socket, request);

	try {
		while (true) {
			if (source_file.eof() == false) {
				source_file.read(buf.c_array(), (std::streamsize)buf.size());
				if (source_file.gcount() <= 0) {
					std::cout << "An error occurred while reading file " << file->getPath() << std::endl;
					return; // throws exception?
				}

				request_stream << SENDING_FILE << "\n\n";
				boost::asio::write(socket, request);
				//Aspetto l'OK del server (serve separare l'invio di SENDING_FILE dall'invio del contenuto)
				boost::array<char, 1024> eat_buf;
				boost::asio::streambuf request_in;
				boost::asio::read_until(socket, request_in, "\n\n");
				std::istream request_stream_in(&request_in);
				int com_code;
				request_stream_in >> com_code;
				request_stream_in.read(eat_buf.c_array(), 2); // eat the "\n\n"
				//Invio il contenuto del file
				if (com_code == OK) {
					boost::asio::write(socket, boost::asio::buffer(buf.c_array(), source_file.gcount()), boost::asio::transfer_all(), error);
					if (error) {
						std::cout << "An error occurred while sending the file content: " << error << std::endl;
						return; // throws exception?
					}
				}
				else {
					std::cout << "Server did not reply with OK" << std::endl;
					return;
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

void sendDir(std::shared_ptr<Directory> dir, tcp::socket& socket, std::string directory_path)
{
	boost::asio::streambuf request;
	std::ostream request_stream(&request);

	request_stream << MK_DIR << "\n" << dir->getPath() << "\n\n";
	boost::asio::write(socket, request); // gestire errori

	auto dir_children = dir->getChildren();
	for (auto it = dir_children.begin(); it != dir_children.end(); ++it) {
		addedElement(it->second, socket, directory_path);
	}
}

void sendModifiedFile(std::shared_ptr<File> file, tcp::socket& socket, std::string directory_path){
	sendFile(file, socket, directory_path);
}

void removedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket)
{
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << RMV_ELEMENT << "\n" << de->getPath() << "\n\n";
	boost::asio::write(socket, request); // gestire errori
}

void addedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket, std::string directory_path)
{
	if (de->type() == 0) { // è directory
		sendDir(std::dynamic_pointer_cast<Directory>(de), socket, directory_path);
	}
	else {
		sendFile(std::dynamic_pointer_cast<File>(de), socket, directory_path);
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

void compareOldNewDir(std::shared_ptr<Directory> old_dir, std::shared_ptr<Directory> new_dir, tcp::socket& socket, std::string directory_path, boost::asio::thread_pool& pool)
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
						compareOldNewDir(old_dir_child, new_dir_child, socket, directory_path, pool);
					}
					else { //Se è un file => invia direttamente il file modificato
						std::shared_ptr<File> file_to_send = std::dynamic_pointer_cast<File>(it_new->second);
#ifdef MULTITHREADING
						boost::asio::post(pool, std::bind(sendModifiedFile, file_to_send, std::move(socket), directory_path));
#else
						sendModifiedFile(file_to_send, socket, directory_path);
#endif
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
//#ifdef MULTITHREADING
			//boost::asio::post(pool, std::bind(addedElement, std::move(addedOrRenamed[i]), std::move(socket)));
//#else
			addedElement(addedOrRenamed[i], socket, directory_path);
//#endif
		}
	}
}

int sendAuthenticationData(const std::string client_name, const std::string hashed_psw, const std::string root_name, tcp::socket& socket)
{
	//Invio dell'autenticazione
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	request_stream_out << START_COMMUNICATION << '\n' << client_name << '\n' << hashed_psw << '\n' << root_name << "\n\n";
	boost::system::error_code error;
	boost::asio::write(socket, request_out, error);

	//Ricezione della risposta
	boost::array<char, 1024> buf;
	boost::asio::streambuf request_in;
	boost::asio::read_until(socket, request_in, "\n\n");
	std::istream request_stream_in(&request_in);
	int com_code;
	request_stream_in >> com_code;
	request_stream_in.read(buf.c_array(), 2); // eat the "\n\n"
	return com_code;
}

void synchronizeElWithServer(std::shared_ptr<DirectoryElement> el, tcp::socket& socket, std::string directory_path, boost::asio::thread_pool& pool)
{
	// Invio checksum e path dir da controllare
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	request_stream_out << VERIFY_CHECKSUM << "\n" << el->getPath() << "\n" << el->getChecksum() << "\n\n";
	boost::asio::write(socket, request_out); // gestire errori

	// Ricevo messaggio dal server
	boost::array<char, 1024> buf;
	boost::asio::streambuf request_in;
	boost::asio::read_until(socket, request_in, "\n\n");
	std::istream request_stream_in(&request_in);
	int com_code;
	request_stream_in >> com_code;
	request_stream_in.read(buf.c_array(), 2); // eat the "\n\n"

	switch (com_code)
	{
		case OK:
			std::cout << "Server is already up to date" << std::endl;
			break;

		case NOT_OK:
			std::cout << "Directory modified: " << el->getPath() << std::endl;
			if (el->type() == 0) { // e' directory con all'interno qualcosa di modificato
				std::shared_ptr<Directory> dir = std::dynamic_pointer_cast<Directory>(el);
				auto dir_children = dir->getChildren();
				for (auto it = dir_children.begin(); it != dir_children.end(); ++it) {
					synchronizeElWithServer(it->second, socket, directory_path, pool);
				}
			}
			else { // e' file modificato
#ifdef MULTITHREADING
				boost::asio::post(pool, std::bind(sendFile, std::dynamic_pointer_cast<File>(el), std::move(socket), directory_path));
#else
				sendFile(std::dynamic_pointer_cast<File>(el), socket, directory_path);
#endif
			}
			break;

		case MISSING_ELEMENT:
			std::cout << "Directory missing: " << el->getPath() << std::endl;
#ifdef MULTITHREADING
			boost::asio::post(pool, std::bind(addedElement, el, std::move(socket), directory_path));
#else
			addedElement(el, socket, directory_path);
#endif
			break;

		default:
			std::cout << "Some weird error happened" << std::endl;
	}
}

void synchronizeWithServer(tcp::socket& socket, std::shared_ptr<Directory> image_root, std::string directory_path, boost::asio::thread_pool& pool)
{
	//Invio il communication code al server
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	request_stream_out << START_SYNC << "\n\n";
	boost::system::error_code error;
	boost::asio::write(socket, request_out, error);
	//Invio la root al server
	std::cout << "Sync with server started" << std::endl;
	synchronizeElWithServer(image_root, socket, directory_path, pool);
	std::cout << "Sync with server completed" << std::endl;
	//Termino la sincronizzazione
	request_stream_out << END_SYNC << "\n\n";
	boost::asio::write(socket, request_out, error);
}

int main()
{
	//Lettura del file di configurazione
	std::ifstream conf_file("../conf.txt");
	std::string name;
	std::string psw;
	std::string root_name;
	std::string directory_path;
	std::string server_ip_port;
	if (conf_file.is_open()) {
		std::cout << "Configuring client..." << std::endl;
		getline(conf_file, name);
		getline(conf_file, psw);
		getline(conf_file, root_name);
		getline(conf_file, directory_path);
		getline(conf_file, server_ip_port);
		conf_file.close();
	}
	else {
		std::cout << "Couldn't open configuration file" << std::endl;
		return -1;
	}
	//Hash della password
	SHA1* psw_sha1 = new SHA1();
	psw_sha1->addBytes(psw.c_str(), strlen(psw.c_str()));
	std::string hashed_psw = psw_sha1->getDigestToHexString();
	//Costruzione dell'immagine
	boost::filesystem::path p(directory_path);
	std::shared_ptr<Directory> image_root = build_dir_wrap(p, root_name); //root dell'immagine del client
	//Connessione al server
	size_t pos = server_ip_port.find(':');
	if (pos == std::string::npos) {
		std::cout << "Invalid format for the ip:port pair" << std::endl;
		return -1;
	}
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
	if (error) {
		std::cout << error.message() << std::endl;
		return -1;
	}
	std::cout << "Connected to " << server_ip_port << std::endl;
	//Invio di name, hashed_psw e root_name al server
	std::cout << "Authenticating..." << std::endl;
	if (sendAuthenticationData(name, hashed_psw, root_name, socket) == NOT_OK) {
		std::cout << "Authentication failed" << std::endl;
		return -1;
	}
	else
		std::cout << "Authentication completed" << std::endl;
	//Sincronizzazione dell'immagine con il server e creazione della thread pool
	boost::asio::thread_pool pool(std::thread::hardware_concurrency());
	synchronizeWithServer(socket, image_root, directory_path, pool);
	//Loop di controllo (deve essere possibile chiuderlo)
	/*while (true) {
		image_root->ls(4);
		std::chrono::milliseconds timespan(1000);
		std::this_thread::sleep_for(timespan);
		std::shared_ptr<Directory> current_root = build_dir_wrap(p); //root della directory corrente
		std::cout << "Checking..." << std::endl;
		compareOldNewDir(image_root, current_root, socket, pool);
		image_root = std::move(current_root); //Aggiornamento dell'immagine
	}*/
	socket.close();
	pool.join();
	return 0;
}