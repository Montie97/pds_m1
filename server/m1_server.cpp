
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/array.hpp>
#include <string>
#include <boost/filesystem.hpp>
#include <mutex>
#include "DirectoryAndFilesAnalyzers/Directory.h"
#include "sha1/sha1.h"
using namespace boost::asio::ip;

std::mutex m_db_file;
unsigned short tcp_port = 1234;
enum CommunicationCodes { START_COMMUNICATION, END_COMMUNICATION, VERIFY_CHECKSUM, OK, NOT_OK, MISSING_ELEMENT, MK_DIR, RMV_ELEMENT, RNM_ELEMENT, START_SEND_FILE, SENDING_FILE, END_SEND_FILE, START_SYNC, END_SYNC, VERSION_MISMATCH };


void out(const std::string& str)
{
	std::cout << str << std::endl;
}

void build_dir(std::shared_ptr<Directory> dir, boost::filesystem::path p)
{
	for (auto x : boost::filesystem::directory_iterator(p)) {
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

std::shared_ptr<Directory> build_dir_wrap(boost::filesystem::path p, const std::string& rootname)
{
	if (boost::filesystem::exists(p)) {
		if (boost::filesystem::is_directory(p)) {
			std::shared_ptr<Directory> root = std::make_shared<Directory>(Directory());
			root->setName(rootname);
			root->setSelf(root);
			build_dir(root, p);
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


/*
	boost::asio::streambuf request;
	std::ostream request_stream(&request);
	request_stream << com_code << "\n" << percorso << "\n" << checksum << "\n\n";
	boost::asio::write(socket, request);
*/

/*
bool compareChecksum(std::shared_ptr<DirectoryElement> old_dir, std::shared_ptr<DirectoryElement> new_dir)
{
	return old_dir->getChecksum() == new_dir->getChecksum();
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
*/

/*
void sendFile(std::shared_ptr<File> file, tcp::socket& socket);

void sendDir(std::shared_ptr<Directory> dir, tcp::socket& socket);

void addedElement(std::shared_ptr<DirectoryElement> de, tcp::socket& socket)
{
	if (de->type() == 0) { // è directory
		sendDir(std::dynamic_pointer_cast<Directory>(de), socket);
	}
	else {
		sendFile(std::dynamic_pointer_cast<File>(de), socket);
	}
}


void sendDir(std::shared_ptr<Directory> dir, tcp::socket& socket)
{
	boost::asio::streambuf request;
	std::ostream request_stream(&request);

	std::cout << "Dir creata: " << dir->getPath() << "\n\n";
	request_stream << MK_DIR << "\n" << dir->getPath() << "\n\n";
	boost::asio::write(socket, request); // gestire errori

	for (auto it = dir->getChildren().begin(); it != dir->getChildren().end(); ++it) {
		addedElement(it->second, socket);
	}
}


void synchronizeElWithServer(std::shared_ptr<DirectoryElement> el, tcp::socket& socket)
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
		std::cout << "Tutto bene!" << std::endl;
		break;

	case NOT_OK:
		std::cout << "Directory modificata: " << el->getPath() << std::endl;
		if (el->type() == 0) { // e' directory con all'interno qualcosa di modificato
			std::shared_ptr<Directory> dir = std::dynamic_pointer_cast<Directory>(el);
			for (auto it = dir->getChildren().begin(); it != dir->getChildren().end(); ++it) {
				synchronizeElWithServer(it->second, socket);
			}
		}
		else { // e' file modificato
			sendFile(std::dynamic_pointer_cast<File>(el), socket);
		}
		break;

	case MISSING_ELEMENT:
		std::cout << "Directory mancante: " << el->getPath() << std::endl;
		addedElement(el, socket);
		break;

	default:
		std::cout << "Qualcosa di molto grave e' accaduto" << std::endl;
	}
}
*/


void startCommunication(tcp::socket& socket, std::shared_ptr<Directory>& root, std::string& username, std::istream& input_request_stream)
{
	std::string hash_psw;
	std::string root_name;
	input_request_stream >> username;
	input_request_stream >> hash_psw;
	input_request_stream >> root_name;

	std::cout << username << " " << hash_psw << " " << root_name << std::endl;

	m_db_file.lock();
	std::ifstream db_file("database.txt");
	if (!db_file.is_open()) {
		std::cerr << "failed to open database file" << std::endl;
		return; // TO DO: GESTIRE ERRORE APERTURA FILE NETWORK
	}

	// Procedura di autenticazione - ricerca di utente in database
	std::string db_curr_username;
	std::string db_curr_hash_psw;
	bool logged = false;
	bool user_found = false;
	while (!db_file.eof()) {
		db_file >> db_curr_username;
		db_file >> db_curr_hash_psw;

		if (db_curr_username == username) {
			user_found = true;
			if (db_curr_hash_psw == hash_psw) {
				logged = true;
			}
			else {
				break;
			}
		}
	}
	db_file.close();

	if (!user_found) { // Utente non trovato - si procede con la registrazione
		std::ofstream db_file_out("database.txt", std::ios_base::binary | std::ios_base::app);
		if (!db_file_out) {
			std::cerr << "failed to open database file output" << std::endl;
		}

		db_file_out << username << " " << hash_psw << "\n";
		db_file_out.close();
		logged = true;
	}
	m_db_file.unlock();

	boost::filesystem::path p(username+"/"+root_name);  // es: alessandro/musica (l'utente "alessandro" salva in remoto la sua cartella locale "musica")

	boost::asio::streambuf output_request;
	std::ostream output_request_stream(&output_request);

	if (logged) {
		boost::filesystem::path user_dir(username);
		if (!boost::filesystem::exists(user_dir))
			boost::filesystem::create_directory(user_dir);

		if (!boost::filesystem::exists(p)) {
			boost::filesystem::create_directory(p);
			output_request_stream << OK << "\n\n"; // era NOT_OK
		}
		else {
			output_request_stream << OK << "\n\n";
		}
		root = build_dir_wrap(p, root_name);
	}
	else {
		output_request_stream << NOT_OK << "\n\n"; // era WRONG_PASSWORD
	}

	boost::asio::write(socket, output_request);
	root->ls(4);
}

void verifyChecksum(tcp::socket& socket, std::shared_ptr<Directory>& root, std::istream& input_request_stream)
{
	std::string path_name;
	std::string checksum;
	input_request_stream >> path_name;
	input_request_stream >> checksum;

	std::shared_ptr<DirectoryElement> de = root->searchDirEl(path_name);

	boost::asio::streambuf output_request;
	std::ostream output_request_stream(&output_request);

	if (de == nullptr) {
		output_request_stream << MISSING_ELEMENT << "\n\n";
	}
	else {
		de->setCheckNotRemovedFlag(true); // flaggo l'elemento appena verificato, significa che esiste ancora lato client
		if (de->getChecksum() != checksum) {
			output_request_stream << NOT_OK << "\n\n";
		}
		else {
			output_request_stream << OK << "\n\n";
		}
	}

	boost::asio::write(socket, output_request);
}

void setNotRemovedFlagsFalse(std::shared_ptr<Directory>& dir)
{
	dir->setCheckNotRemovedFlag(false);
	auto dir_children = dir->getChildren(); // boh
	for (auto it = dir_children.begin(); it != dir_children.end(); ++it) {
		if (it->second->type() == 0) { // e' dir
			std::shared_ptr<Directory> sdir = std::dynamic_pointer_cast<Directory>(it->second);
			setNotRemovedFlagsFalse(sdir);
		}
		else { // e' file
			it->second->setCheckNotRemovedFlag(false);
		}
	}
}

void removeNotFlaggedElements(std::shared_ptr<Directory>& root, std::shared_ptr<Directory>& dir)
{
	if (dir->getCheckNotRemovedFlag() == false) {
		root->remove(dir->getPath().substr(dir->getPath().find_first_of("/"), dir->getPath().length())); // rimuove la root dal path
	}
	else {
		auto dir_children = dir->getChildren(); // boh
		for (auto it = dir_children.begin(); it != dir_children.end(); ++it) {
			if (it->second->type() == 0) { // e' dir
				std::shared_ptr<Directory> sdir = std::dynamic_pointer_cast<Directory>(it->second);
				removeNotFlaggedElements(root, sdir);
			}
			else { // e' file
				if (it->second->getCheckNotRemovedFlag() == false) {
					root->remove(it->second->getPath().substr(it->second->getPath().find_first_of("/"), it->second->getPath().length())); // rimuove la root dal path

				}
			}
		}
	}
}

void mkDir(tcp::socket& socket, std::shared_ptr<Directory>& root, const std::string& username, std::istream& input_request_stream)
{
	std::string path_name;
	input_request_stream >> path_name;

	boost::filesystem::path p(username + "/" + path_name);
	boost::filesystem::create_directory(p);

	std::shared_ptr<Directory> ptr = root->addDirectory(path_name);
	if (!ptr) {
		out("ECCEZIONAZZA: per quale motivo la dir non è stata creata?");
	}
}

void rmvEl(tcp::socket& socket, std::shared_ptr<Directory>& root, const std::string& username, std::istream& input_request_stream)
{
	std::string path_name;
	input_request_stream >> path_name;

	boost::filesystem::path p(username + "/" + path_name);
	
	if (boost::filesystem::exists(p)) {
		boost::filesystem::remove(p);

		if (!root->remove(path_name)) {
			out("ECCEZIONE: percorso sbagliato o file inesistente");
		}
	}
	else {
		out("File inesistente");
	}
}

void rnmEl(tcp::socket& socket, std::shared_ptr<Directory>& root, const std::string& username, std::istream& input_request_stream)
{
	std::string path_old_name;
	std::string path_new_name;
	input_request_stream >> path_old_name;
	input_request_stream >> path_new_name;

	boost::filesystem::path p(username + "/" + path_old_name);

	if (boost::filesystem::exists(p)) {
		boost::filesystem::rename(path_old_name, path_new_name);

		if (!root->rename(path_old_name, path_new_name)) {
			out("ECCEZIONE: percorso sbagliato o file inesistente");
		}
	}
	else {
		out("File inesistente");
	}
}

void startSendingFile(tcp::socket& socket, std::shared_ptr<Directory>& root, const std::string& username, std::istream& input_request_stream)
{
	// Inizializzazione variabili
	boost::array<char, 1024> buf;
	std::string file_path;
	size_t file_size = -1;
	time_t last_edit; // TODO: AGGIORNARE ROOT CON NUOVO FILE
	boost::system::error_code error;

	// ricezione path file e dimensione
	input_request_stream >> file_path;
	input_request_stream >> file_size;
	input_request_stream >> last_edit;
	input_request_stream.read(buf.c_array(), 2); // eat the "\n\n"

	std::cout << file_path << " size is " << file_size << ", last edited: " << last_edit << std::endl;

	std::ofstream output_file(username + "/" + root->getName() + "/" + file_path.c_str(), std::ios_base::binary);
	
	if (!output_file) {
		std::cout << "ECCEZIONE: failed to open " << file_path << std::endl;
	}

	while (true) {
		// Inizializzazione dei buffer necessari per ricevere il messaggio di trasmissione file intermedio
		// (SENDING_FILE o END_SEND_FILE)
		boost::asio::streambuf request_buf;
		boost::asio::read_until(socket, request_buf, "\n\n");
		std::cout << "request size: " << request_buf.size() << "\n";
		std::istream input_request_stream(&request_buf);
		int com_code;
		input_request_stream >> com_code;
		input_request_stream.read(buf.c_array(), 2); // eat the "\n\n"

		// Invio ACK
		boost::asio::streambuf request_out;
		std::ostream request_stream_out(&request_out);
		request_stream_out << OK << "\n\n";
		boost::asio::write(socket, request_out); // gestire errori

		if (com_code == SENDING_FILE) {
			// Legge bytes trasmessi e li scrive su file
			size_t len = socket.read_some(boost::asio::buffer(buf), error);

			std::cout << "Received chunk bytes: " << len << std::endl;
			if (len > 0) {
				output_file.write(buf.c_array(), (std::streamsize) len);
			}
			if (error) {
				std::cout << "error: " << error.message() << std::endl;
				break;
			}
		}
		else if (com_code == END_SEND_FILE) {
			// A fine trasmissione, controlla la dimensione file per capire se qualcosa è cambiato durante la trasmissione
			// in fine chiude il file
			if (output_file.tellp() == (std::fstream::pos_type)(std::streamsize) file_size) {
				out("file ricevuto senza problemi");
			}
			else {
				out("dimensione file è cambiata durante la trasmissione, o errore trasmissione file_size");
			}
			
			//se il file è arrivato si aggiorna il filesystem
			root->remove(file_path); // se c'era già, lo rimuove
			std::shared_ptr<File> ptr = root->addFile(file_path, file_size, last_edit);
			if (!ptr) {
				out("ECCEZIONAZZA: per quale motivo il file non è stato creato?");
			}
			boost::filesystem::path p(username + "/" + root->getName() + "/" + file_path.c_str());
			boost::filesystem::last_write_time(p, last_edit);

			//chiusura file
			std::cout << "received " << output_file.tellp() << " bytes.\n";
			output_file.close();
			break;
		}
	}
}

void clientHandler(tcp::socket& socket)
{
	bool quit = false;
	std::shared_ptr<Directory> root;
	std::string user;

	while (!quit) {
		boost::array<char, 1024> buf;
		out("waiting for message");

		boost::asio::streambuf request_buf;
		boost::system::error_code error;
		boost::asio::read_until(socket, request_buf, "\n\n", error);
		if (error) {
			std::cout << "Problema di connessione: " << error.message() << ", spegnimento client" << std::endl;
			quit = true;
		}
		else {
			std::cout << "request size: " << request_buf.size() << "\n";
			std::istream request_stream(&request_buf);
			int com_code;
			request_stream >> com_code;

			std::cout << "com_code: " << com_code << std::endl;

			switch (com_code) {
				case START_COMMUNICATION:
					out("Start communication");
					startCommunication(socket, root, user, request_stream);
					break;

				case VERIFY_CHECKSUM:
					out("verify checksum");
					verifyChecksum(socket, root, request_stream);
					break;

				case START_SYNC:
					out("sync started");
					setNotRemovedFlagsFalse(root);
					break;

				case END_SYNC:
					out("sync ended");
					removeNotFlaggedElements(root, root); // elimina tutti gli elementi non flaggati
					break;

				case MK_DIR:
					mkDir(socket, root, user, request_stream);
					root->calculateChecksum();
					root->ls(0);
					break;

				case RMV_ELEMENT:
					rmvEl(socket, root, user, request_stream);
					root->calculateChecksum();
					root->ls(0);
					break;

				case RNM_ELEMENT:
					rnmEl(socket, root, user, request_stream);
					root->calculateChecksum();
					root->ls(0);
					break;

				case START_SEND_FILE:
					startSendingFile(socket, root, user, request_stream);
					root->calculateChecksum();
					root->ls(0);
					break;

				case END_COMMUNICATION:
					quit = true;
					break;

				default:
					out("che succede?");
					break;
			}

			request_stream.read(buf.c_array(), 2); // eat the "\n\n"
		}
	}
}

int main()
{
	/*std::string folder_path = "../_test_folder";
	boost::filesystem::path p(folder_path);
	std::shared_ptr<Directory> root = build_dir_wrap(p);
	root->ls(0);
	return 5;*/

	boost::filesystem::path p("montie/root/a/2.txt");
	std::cout << boost::filesystem::last_write_time(p) << std::endl;
	boost::filesystem::last_write_time(p, 1598022125);
	std::cout << boost::filesystem::last_write_time(p) << std::endl;
	return 0;

	// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


	std::ifstream config_file("config.txt");
	if (!config_file.is_open()) {
		std::cerr << "failed to open file" << std::endl;
		return -1;
	}

	int n_threads_in_threadpool;
	config_file >> n_threads_in_threadpool;

	boost::asio::thread_pool pool(n_threads_in_threadpool);
	std::cout << "Threads in thread_pool: " << n_threads_in_threadpool << std::endl;
	
	try {
		std::cout << "Server listening on port: " << tcp_port << std::endl;

		boost::asio::io_service io_service;
		boost::asio::ip::tcp::acceptor acceptor(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), tcp_port));

		boost::system::error_code error;
		
		while (true) {
			boost::asio::ip::tcp::socket socket(io_service);
			acceptor.accept(socket, error);

			if (error) {
				std::cout << error << std::endl;
				break; // comportamento da modificare
			}
			else {
				std::cout << "Got client connection." << std::endl;
				boost::asio::post(pool, std::bind(clientHandler, std::move(socket)));
			}
		}
	}
	catch (std::exception & e) {
		std::cerr << e.what() << std::endl;
	}

	pool.join();
	return 0;
}
