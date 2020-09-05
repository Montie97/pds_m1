#include <iostream>
#include <fstream>
#include <boost/filesystem.hpp>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include "Directory.h"

//#define MULTITHREADING

#ifdef MULTITHREADING
#include <thread>
#include <future>
#endif

#define PROTOCOL_VERSION 5

using boost::asio::ip::tcp;

enum ErrorCodes { SYNC_ERR, CHECKSUM_ERR, RNM_ERR, RMV_ERR, SEND_DIR_ERR, OPEN_FILE_ERR, START_SEND_FILE_ERR, READ_FILE_ERR, SEND_CHUNK_ERR, SEND_FILE_ERR, END_SEND_FILE_ERR, CONF_ERR, NOT_EXISTING_DIR_ERR, ADD_DIR_ERR, ADD_FILE_ERR, CONF_FILE_ERR, IP_PORT_ERR, CONNECTION_ERR, AUTH_ERR, BOOST_ERR, VERSION_ERR };
enum CommunicationCodes { START_COMMUNICATION, VERIFY_CHECKSUM, OK, NOT_OK, MISSING_ELEMENT, MK_DIR, RMV_ELEMENT, RNM_ELEMENT, START_SEND_FILE, SENDING_FILE, END_SEND_FILE, START_SYNC, END_SYNC, VERSION_MISMATCH };

void addedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket, std::string directory_path); //Serve il prototipo perché c'è una ricorsione "indiretta" in sendDir" (sendDir chiama addedElement e addedElement chiama sendDir)

void errorMessage(int err, std::string details)
{
	switch (err) {

		case CONF_FILE_ERR:
			std::cout << "Couldn't open configuration file" << std::endl;
			break;

		case CONF_ERR:
			std::cout << "Must specify a directory in the configuration file" << std::endl;
			break;

		case NOT_EXISTING_DIR_ERR:
			std::cout << "The path specified in the configuration file does not exist" << std::endl;
			break;

		case ADD_DIR_ERR:
			std::cout << "An error occurred while adding a directory to the local image" << std::endl;
			break;

		case ADD_FILE_ERR:
			std::cout << "An error occurred while adding a file to the local image" << std::endl;
			break;

		case IP_PORT_ERR:
			std::cout << "Invalid format for the ip:port pair" << std::endl;
			break;

		case CONNECTION_ERR:
			std::cout << "An error occurred while trying to connect to server" << std::endl;
			break;

		case AUTH_ERR:
			std::cout << "Authentication failed" << std::endl;
			break;

		case SYNC_ERR:
			std::cout << "An error occurred during the synchronization" << std::endl;
			break;

		case CHECKSUM_ERR:
			std::cout << "An error occurred during the verification of a checksum (" << details << ")" << std::endl;
			break;

		case RNM_ERR:
			std::cout << "An error occurred during the notification regarding a renamed element (" << details << ")" << std::endl;
			break;

		case RMV_ERR:
			std::cout << "An error occurred during the notification regarding a removed element (" << details << ")" << std::endl;
			break;

		case SEND_DIR_ERR:
			std::cout << "An error occurred while sending a directory (" << details << ")" << std::endl;
			break;

		case OPEN_FILE_ERR:
			std::cout << "An error occurred while trying to open a file to send (" << details << ")" << std::endl;
			break;

		case START_SEND_FILE_ERR:
			std::cout << "An error occurred during the beginning of the file sending process (" << details << ")" << std::endl;
			break;

		case READ_FILE_ERR:
			std::cout << "An error occurred while reding the content of a file (" << details << ")" << std::endl;
			break;

		case SEND_FILE_ERR:
			std::cout << "An error occurred while notifying the server of the beginning of the chunks sending process (" << details << ")" << std::endl;
			break;

		case SEND_CHUNK_ERR:
			std::cout << "An error occurred while sending a chunk of file to the server" << std::endl;
			break;

		case END_SEND_FILE_ERR:
			std::cout << "An error occurred while terminating the file sending process (" << details << ")" << std::endl;
			break;

		case BOOST_ERR:
			std::cout << "An error occured during a boost::write operation on a socket" << std::endl;
			break;

		case VERSION_ERR:
			std::cout << "The protocol version of the client is not up to date" << std::endl;
			break;
	}
}

int receiveCodeFromServer(tcp::socket& socket)
{
	boost::array<char, 1024> buf;
	boost::asio::streambuf request_in;
	boost::asio::read_until(socket, request_in, "\n\n");
	std::istream request_stream_in(&request_in);
	int com_code;
	request_stream_in >> com_code;
	request_stream_in.read(buf.c_array(), 2); // eat the "\n\n"
	return com_code;
}

void build_dir(std::shared_ptr<Directory> dir, boost::filesystem::path p)
{
	for (auto x : boost::filesystem::directory_iterator(p)) {
		//Se l'elemento è una directory, il processo si ripete ricorsivamente
		if (boost::filesystem::is_directory(x.path())) {
			std::shared_ptr<Directory> new_dir = dir->addDirectory(x.path().filename().string());
			if (new_dir == nullptr)
				throw ADD_DIR_ERR;
			build_dir(new_dir, x.path());
		}
		else {
			std::shared_ptr<File> new_file = dir->addFile(x.path().filename().string(), boost::filesystem::file_size(x.path()), boost::filesystem::last_write_time(x.path()));
			if (new_file == nullptr)
				throw ADD_FILE_ERR;
		}
	}
}

std::shared_ptr<Directory> build_dir_wrap(boost::filesystem::path p, std::string root_name)
{
	try {
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
				throw CONF_ERR;
		}
		else
			throw NOT_EXISTING_DIR_ERR;
	}
	catch (int err) {
		errorMessage(err, "");
		return std::shared_ptr<Directory>(nullptr);
	}
}

bool compareChecksum(std::shared_ptr<DirectoryElement> old_dir, std::shared_ptr<DirectoryElement> new_dir) {
	return old_dir->getChecksum() == new_dir->getChecksum();
}

void sendFile(std::shared_ptr<File> file, tcp::socket& socket, std::string directory_path)
{
	std::cout << "Sending file " << file->getPath() << std::endl;
	//Apertura del file
	boost::system::error_code error;
	boost::array<char, 65536> buf; //Buffer usato per inviare i chunk di file al server
	std::ifstream source_file(directory_path + '/' + file->getPath(), std::ios_base::binary | std::ios_base::ate);
	try {
		if (!source_file.is_open())
			throw OPEN_FILE_ERR;
		//Lettura delle informazioni sul file
		std::string file_path = file->getPath();
		std::time_t last_edit = file->getLastEdit();
		size_t file_size = source_file.tellg();
		source_file.seekg(0);

		//Si avvisa il server che sta cominciando la serie di operazioni di invio di un file
		boost::asio::streambuf request;
		std::ostream request_stream(&request);
		replace(file_path.begin(), file_path.end(), ' ', '?'); //Sostituisco gli spazi nel path con i punti interrogativi
		request_stream << START_SEND_FILE << "\n" << file_path << "\n" << file_size << "\n" << last_edit << "\n\n";
		boost::system::error_code error;
		boost::asio::write(socket, request, error);
		if (error) {
			source_file.close();
			std::cout << error.message() << std::endl;
			throw BOOST_ERR;
		}

		//Attesa dell'ACK
		if (receiveCodeFromServer(socket) != OK) {
			source_file.close();
			throw START_SEND_FILE_ERR;
		}

		//Se il file non è terminato e se non è vuoto => ne leggo un chunk e lo invio al server
		while (source_file.eof() == false && file_size != 0) {
			source_file.read(buf.c_array(), (std::streamsize)buf.size());
			if (source_file.gcount() <= 0) {
				source_file.close();
				throw READ_FILE_ERR;
			}

			//Avviso il server che sto per inviargli un chunk di file
			request_stream << SENDING_FILE << '\n' << source_file.gcount() << "\n\n";
			boost::asio::write(socket, request, error);
			if (error) {
				source_file.close();
				std::cout << error.message() << std::endl;
				throw BOOST_ERR;
			}

			//Aspetto l'OK del server (serve separare l'invio di SENDING_FILE dall'invio del contenuto)
			if (receiveCodeFromServer(socket) == OK) {
				//Invio un chunk del file
				boost::asio::write(socket, boost::asio::buffer(buf.c_array(), source_file.gcount()), boost::asio::transfer_all(), error);
				if (error) {
					source_file.close();
					throw BOOST_ERR;
				}
			}
			else {
				source_file.close();
				throw SEND_FILE_ERR;
			}

			//Aspetto che il server mi dica di aver ricevuto il chunk
			if (receiveCodeFromServer(socket) != OK) {
				source_file.close();
				throw SEND_CHUNK_ERR;
			}
		}
		//Comunico al server che la procedura di invio del file è terminata
		request_stream << END_SEND_FILE << '\n' << 0 << "\n\n";
		boost::asio::write(socket, request, error);
		if (error) {
			std::cout << error.message() << std::endl;
			throw BOOST_ERR;
		}
		if (receiveCodeFromServer(socket) != OK)
			throw END_SEND_FILE_ERR;
	}
	catch (enum ErrorCodes err) {
		source_file.close();
		errorMessage(err, file->getPath());
		throw false; //Questo throw viene usato per segnalare alla funzione chiamante che l'eccezione è già stata catturata
	}
	source_file.close();
}

void sendDir(std::shared_ptr<Directory> dir, tcp::socket& socket, std::string directory_path)
{
	std::cout << "Sending dir " << dir->getPath() << std::endl;
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	std::string path = dir->getPath();
	replace(path.begin(), path.end(), ' ', '?'); //Sostituisco gli spazi nel path con i punti interrogativi
	request_stream << MK_DIR << "\n" << path << "\n\n";
	boost::system::error_code error;
	boost::asio::write(socket, request, error);

	try {
		if (error) {
			std::cout << error.message() << std::endl;
			throw BOOST_ERR;
		}
		if (receiveCodeFromServer(socket) == OK) {
			auto dir_children = dir->getChildren();
			for (auto it = dir_children.begin(); it != dir_children.end(); ++it) {
				addedElement(it->second, socket, directory_path);
			}
		}
		else
			throw SEND_DIR_ERR;
	}
	catch (enum ErrorCodes err) {
		errorMessage(err, dir->getPath());
		throw false;
	}
	catch (bool e) {
		throw false;
	}
}

void sendModifiedFile(std::shared_ptr<File> file, tcp::socket& socket, std::string directory_path) {
	sendFile(file, socket, directory_path);
}

void removedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket)
{
	std::cout << "Removing " << de->getPath() << std::endl;
	try {
		boost::asio::streambuf request;
		std::ostream request_stream(&request);
		std::string path = de->getPath();
		replace(path.begin(), path.end(), ' ', '?'); //Sostituisco gli spazi nel path con i punti interrogativi
		request_stream << RMV_ELEMENT << "\n" << path << "\n\n";
		boost::system::error_code error;
		boost::asio::write(socket, request, error);
		if (error) {
			std::cout << error.message() << std::endl;
			throw BOOST_ERR;
		}
		if (receiveCodeFromServer(socket) != OK)
			throw RMV_ERR;
	}
	catch (enum ErrorCodes err) {
		errorMessage(err, de->getPath());
		throw false;
	}
}

void addedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket, std::string directory_path)
{
	std::cout << "Adding " << de->getPath() << std::endl;
	if (de->type() == 0) { // è directory
		sendDir(std::dynamic_pointer_cast<Directory>(de), socket, directory_path);
	}
	else {
		sendFile(std::dynamic_pointer_cast<File>(de), socket, directory_path);
	}
}

void renamedElement(std::shared_ptr<DirectoryElement> de1, std::shared_ptr<DirectoryElement> de2, tcp::socket& socket)
{
	std::cout << "Renaming " << de1->getPath() << " into " << de2->getPath() << std::endl;
	try {
		boost::asio::streambuf request;
		std::ostream request_stream(&request);
		std::string path1 = de1->getPath();
		std::string path2 = de2->getPath();
		replace(path1.begin(), path1.end(), ' ', '?'); //Sostituisco gli spazi nel path con i punti interrogativi
		replace(path2.begin(), path2.end(), ' ', '?'); //Sostituisco gli spazi nel path con i punti interrogativi
		request_stream << RNM_ELEMENT << "\n" << path1 << "\n" << path2 << "\n\n";
		boost::system::error_code error;
		boost::asio::write(socket, request, error);
		if (error) {
			std::cout << error.message() << std::endl;
			throw BOOST_ERR;
		}
		if (receiveCodeFromServer(socket) != OK)
			throw RNM_ERR;
	}
	catch (enum ErrorCodes err) {
		errorMessage(err, de1->getPath() + " -> " + de2->getPath());
		throw false;
	}
}

bool checkRenamed(std::shared_ptr<Directory> dir1, std::shared_ptr<Directory> dir2)
{
	auto children = dir1->getChildren();
	// se il numero di file all'interno delle dir è diverso, ritorna false
	if (children.size() != children.size()) {
		return false;
	}

	for (auto it = children.begin(); it != children.end(); ++it)
	{
		if (children.count(it->first) > 0) { // il nome esiste in dir2
			if (!compareChecksum(it->second, children[it->first])) { // se checksum disuguali ritorna false
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

void compareOldNewDir(std::shared_ptr<Directory> old_dir, std::shared_ptr<Directory> new_dir, tcp::socket& socket, std::string directory_path)
{
	std::vector<std::shared_ptr<DirectoryElement>> removedOrRenamed;
	std::vector<std::shared_ptr<DirectoryElement>> addedOrRenamed;

	try {
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
							compareOldNewDir(old_dir_child, new_dir_child, socket, directory_path);
						}
						else { //Se è un file => invia direttamente il file modificato
							std::shared_ptr<File> file_to_send = std::dynamic_pointer_cast<File>(it_new->second);
							sendModifiedFile(file_to_send, socket, directory_path);
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
					//Il checkRenamed va solo fatto per le directory
					if ((removedOrRenamed[i]->type() == 0 && addedOrRenamed[j]->type() == 0) && (checkRenamed(std::dynamic_pointer_cast<Directory>(removedOrRenamed[i]), std::dynamic_pointer_cast<Directory>(addedOrRenamed[j])))) {
						renamedElement(std::dynamic_pointer_cast<Directory>(removedOrRenamed[i]), std::dynamic_pointer_cast<Directory>(addedOrRenamed[j]), socket);
						removedOrRenamed.erase(it_i);
						addedOrRenamed.erase(it_j);
					}
					else {
						it_i++;
						it_j++;
					}
				}
			}

			for (size_t i = 0; i < removedOrRenamed.size(); i++) {
				removedElement(removedOrRenamed[i], socket);
			}
			for (size_t i = 0; i < addedOrRenamed.size(); i++) {
				addedElement(addedOrRenamed[i], socket, directory_path);
			}
		}
	}
	catch (enum ErrorCodes e) {
		throw false;
	}
}

int sendAuthenticationData(const std::string client_name, const std::string hashed_psw, const std::string root_name, tcp::socket& socket)
{
	//Invio dell'autenticazione e della versione del protocollo
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	request_stream_out << START_COMMUNICATION << '\n' << PROTOCOL_VERSION << '\n' << client_name << '\n' << hashed_psw << '\n' << root_name << "\n\n";
	boost::system::error_code error;
	boost::asio::write(socket, request_out, error);
	try {
		if (error) {
			std::cout << error.message() << std::endl;
			throw BOOST_ERR;
		}
	}
	catch (enum ErrorCodes err) {
		errorMessage(err, "");
		return NOT_OK;
	}

	//Ricezione della risposta
	return receiveCodeFromServer(socket);
}

void synchronizeElWithServer(std::shared_ptr<DirectoryElement> el, tcp::socket& socket, std::string directory_path)
{
	// Invio checksum e path dir da controllare
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	std::string path = el->getPath();
	replace(path.begin(), path.end(), ' ', '?');
	request_stream_out << VERIFY_CHECKSUM << "\n" << path << "\n" << el->getChecksum() << "\n\n";
	boost::system::error_code error;
	boost::asio::write(socket, request_out, error); // gestire errori

	try {
		if (error) {
			std::cout << error.message() << std::endl;
			throw BOOST_ERR;
		}

		switch (receiveCodeFromServer(socket))
		{
			case OK:
				//std::cout << "Server is already up to date" << std::endl;
				break;

			case NOT_OK:
				//std::cout << "Directory modified: " << el->getPath() << std::endl;
				if (el->type() == 0) { // e' directory con all'interno qualcosa di modificato
					std::shared_ptr<Directory> dir = std::dynamic_pointer_cast<Directory>(el);
					auto dir_children = dir->getChildren();
					for (auto it = dir_children.begin(); it != dir_children.end(); ++it) {
						synchronizeElWithServer(it->second, socket, directory_path);
					}
				}
				else { // e' file modificato
					sendFile(std::dynamic_pointer_cast<File>(el), socket, directory_path);
				}
				break;

			case MISSING_ELEMENT:
				//std::cout << "Directory missing: " << el->getPath() << std::endl;
				addedElement(el, socket, directory_path);
				break;

			default:
				throw CHECKSUM_ERR;
		}
	}
	catch (enum ErrorCodes err) {
		errorMessage(err, el->getPath());
		throw false;
	}
	catch (bool e) {
		throw false;
	}
}

void synchronizeWithServer(tcp::socket& socket, std::shared_ptr<Directory> image_root, std::string directory_path)
{
	//Invio il communication code al server
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	request_stream_out << START_SYNC << "\n\n";
	boost::system::error_code error;
	boost::asio::write(socket, request_out, error);
	try{
		if (error) {
			std::cout << error.message() << std::endl;
			throw BOOST_ERR;
		}
	
		if (receiveCodeFromServer(socket) == OK) {
			//Invio la root al server
			std::cout << "Sync with server started" << std::endl;
			synchronizeElWithServer(image_root, socket, directory_path);
			std::cout << "Sync with server completed" << std::endl;
			//Termino la sincronizzazione
			request_stream_out << END_SYNC << "\n\n";
			boost::asio::write(socket, request_out, error);
		}
		else
			throw SYNC_ERR;
	}
	catch (enum ErrorCodes err) {
		errorMessage(err, "");
		throw false;
	}
	catch (bool e) {
		throw false;
	}
}

void connectAndAuthenticate(tcp::socket& socket, const std::string& server_ip_port, const std::string& username, const std::string& hashed_psw, const std::string& root_name, tcp::resolver::iterator& endpoint_iterator, tcp::resolver::iterator& end)
{
	boost::system::error_code error = boost::asio::error::host_not_found;
	while (error && endpoint_iterator != end)
	{
		socket.close();
		socket.connect(*endpoint_iterator++, error);
	}
	try {
		if (error) {
			std::cout << error.message() << std::endl;
			throw CONNECTION_ERR;
		}
		std::cout << "Connected to " << server_ip_port << std::endl;
		//Invio di name, hashed_psw e root_name al server (autenticazione)
		std::cout << "Authenticating..." << std::endl;
		switch (sendAuthenticationData(username, hashed_psw, root_name, socket)) {
			case OK:
				std::cout << "Authentication completed" << std::endl;
				break;
			case NOT_OK:
				throw AUTH_ERR;
			case VERSION_MISMATCH:
				throw VERSION_ERR;
		}	
	}
	catch (enum ErrorCodes err) {
		errorMessage(err, "");
		throw false;
	}
}

int main()
{
	//Lettura del file di configurazione
	std::ifstream conf_file("conf.txt");
	std::string name;
	std::string psw;
	std::string root_name;
	std::string directory_path;
	std::string server_ip_port;
	try {
		if (conf_file.is_open()) {
			std::cout << "Configuring client..." << std::endl;
			getline(conf_file, name);
			getline(conf_file, psw);
			getline(conf_file, root_name);
			getline(conf_file, directory_path);
			getline(conf_file, server_ip_port);
			conf_file.close();
		}
		else
			throw CONF_FILE_ERR;
	}
	catch (enum ErrorCodes err) {
		errorMessage(err, "");
		std::system("pause");
		return -1;
	}
	//Hash della password
	SHA1* psw_sha1 = new SHA1();
	psw_sha1->addBytes(psw.c_str(), strlen(psw.c_str()));
	std::string hashed_psw = psw_sha1->getDigestToHexString();

	try {
		//Connessione al server e autenticazione (fatte da un altro thread mentre quello principale costruisce l'immagine)
		size_t pos = server_ip_port.find(':');
		if (pos == std::string::npos)
			throw IP_PORT_ERR;
		std::string server_port = server_ip_port.substr(pos + 1);
		std::string server_ip = server_ip_port.substr(0, pos);
		boost::asio::io_service io_service;
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(server_ip, server_port);
		tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
		tcp::resolver::iterator end;
		tcp::socket socket(io_service);
#ifdef MULTITHREADING
		std::thread t(connectAndAuthenticate, socket, server_ip_port, name, hashed_psw, root_name, endpoint_iterator, end); //Questa cosa non compila
#else
		connectAndAuthenticate(socket, server_ip_port, name, hashed_psw, root_name, endpoint_iterator, end);
#endif

		//Costruzione dell'immagine
		boost::filesystem::path p(directory_path);
		std::shared_ptr<Directory> image_root = build_dir_wrap(p, root_name); //root dell'immagine del client
		if (image_root == nullptr) {
			std::system("pause");
			return -1; //Si è verificata un'eccezione in build_dir_wrap
	}
#ifdef MULTITHREADING
		t.join();
#endif

		//Sincronizzazione dell'immagine con il server
		synchronizeWithServer(socket, image_root, directory_path);
		//Loop di controllo
		while (true) {
			//image_root->ls(4);
			std::chrono::milliseconds timespan(1000);
			std::this_thread::sleep_for(timespan);
			std::shared_ptr<Directory> current_root = build_dir_wrap(p, root_name); //root della directory corrente
			if (current_root == nullptr)
				return -1; //Si è verificata un'eccezione in build_dir_wrap
			//std::cout << "Checking..." << std::endl;
			compareOldNewDir(image_root, current_root, socket, directory_path);
			image_root.reset();
			image_root = std::move(current_root); //Aggiornamento dell'immagine
		}
		socket.close();
	}
	catch (bool e) {
		std::system("pause");
		return -1;
	}
	std::system("pause");
	return 0;
}