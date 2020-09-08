#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/array.hpp>
#include <string>
#include <boost/filesystem.hpp>
#include <mutex>
#include "Directory.h"
#include "sha1.h"
using namespace boost::asio::ip;

std::mutex m_db;
#define PROTOCOL_VERSION 6
std::vector<std::string> logged_users;
enum CommunicationCodes { START_COMMUNICATION, VERIFY_CHECKSUM, OK, NOT_OK, MISSING_ELEMENT, MK_DIR, RMV_ELEMENT, RNM_ELEMENT, START_SEND_FILE, SENDING_FILE, END_SEND_FILE, START_SYNC, END_SYNC, VERSION_MISMATCH };



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
			root->setIsRoot(true);
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


void ACK(tcp::socket& socket)
{
	boost::system::error_code wer;
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	request_stream_out << OK << "\n\n";
	boost::asio::write(socket, request_out, wer);
	if (wer) {
		std::cout << "wer_ok: " << wer.message() << std::endl;
		throw wer;
	}
}

void sendNotOK(tcp::socket& socket)
{
	boost::system::error_code wer;
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	request_stream_out << NOT_OK << "\n\n";
	boost::asio::write(socket, request_out, wer);
	if (wer) {
		std::cout << "wer_not_ok: " << wer.message() << std::endl;
		throw wer;
	}
}

void startCommunication(tcp::socket& socket, std::shared_ptr<Directory>& root, std::string& username, std::istream& input_request_stream)
{
	boost::system::error_code error;
	std::string hash_psw;
	std::string root_name;
	int p_vers;
	input_request_stream >> p_vers;

	if (p_vers != PROTOCOL_VERSION) {
		boost::asio::streambuf output_request;
		std::ostream output_request_stream(&output_request);
		std::cout << "client connected with version: " << p_vers << " - version required: " << PROTOCOL_VERSION << std::endl;
		
		output_request_stream << VERSION_MISMATCH << "\n\n";
		boost::asio::write(socket, output_request, error);
		if (error) {
			throw error;
		}

		output_request_stream << PROTOCOL_VERSION << "\n\n";
		boost::asio::write(socket, output_request, error);
		if (error) {
			throw error;
		}
	}
	else {
		std::lock_guard<std::mutex> lg(m_db);

		input_request_stream >> username;
		if (std::count(logged_users.begin(), logged_users.end(), username)) {
			boost::asio::streambuf output_request;
			std::ostream output_request_stream(&output_request);
			std::cout << "refused connection: " << username << " already connected" << std::endl;
			output_request_stream << NOT_OK << "\n\n";
			boost::asio::write(socket, output_request, error);
			username = "";
			if (error) {
				throw error;
			}
		}
		else {
			input_request_stream >> hash_psw;
			input_request_stream >> root_name;
			std::replace(root_name.begin(), root_name.end(), '?', ' ');

			std::cout << username << " " << hash_psw << " " << root_name << std::endl;

			
			std::ifstream db_file("database.txt");
			if (!db_file.is_open()) {
				username = "";
				throw std::exception("start com - failed to open database file input");
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
						logged_users.push_back(username);
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
					username = "";
					throw std::exception("start com - failed to open database file output");
				}

				db_file_out << username << " " << hash_psw << "\n";
				db_file_out.close();
				logged = true;
				logged_users.push_back(username);
			}

			boost::filesystem::path p(username + "/" + root_name);  // es: alessandro/musica (l'utente "alessandro" salva in remoto la sua cartella locale "musica")

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
				root->ls(4);
			}
			else {
				output_request_stream << NOT_OK << "\n\n"; // era WRONG_PASSWORD
				username = "";
			}

			boost::asio::write(socket, output_request, error);
			if (error) {
				throw error;
			}
		}
	}
}

void setNotRemovedFlagsRecursive(std::shared_ptr<DirectoryElement> el, bool b)
{
	el->setCheckNotRemovedFlag(b);
	if (el->type() == 0) { // e' dir
		std::shared_ptr<Directory> dir = std::dynamic_pointer_cast<Directory>(el);
		auto dir_children = dir->getChildren(); // boh
		for (auto it = dir_children.begin(); it != dir_children.end(); ++it) {
			if (it->second->type() == 0) { // e' dir
				setNotRemovedFlagsRecursive(it->second, b);
			}
			else { // e' file
				it->second->setCheckNotRemovedFlag(b);
			}
		}
	}
}

void verifyChecksum(tcp::socket& socket, std::shared_ptr<Directory>& root, std::istream& input_request_stream)
{
	boost::system::error_code error;
	std::string path_name;
	std::string checksum;
	input_request_stream >> path_name;
	std::replace(path_name.begin(), path_name.end(), '?', ' ');
	input_request_stream >> checksum;

	std::cout << "verify: " << path_name << std::endl;
	std::shared_ptr<DirectoryElement> de = root->searchDirEl(path_name);

	boost::asio::streambuf output_request;
	std::ostream output_request_stream(&output_request);

	if (de == nullptr) {
		output_request_stream << MISSING_ELEMENT << "\n\n";
	}
	else {
		if (de->getChecksum() != checksum) {
			output_request_stream << NOT_OK << "\n\n";
			de->setCheckNotRemovedFlag(true); // flaggo l'elemento appena verificato, significa che esiste ancora lato client
		}
		else {
			output_request_stream << OK << "\n\n";
			setNotRemovedFlagsRecursive(de, true);
		}
	}

	boost::asio::write(socket, output_request, error);
	if (error) {
		throw error;
	}
}

void removeNotFlaggedElements(std::shared_ptr<Directory>& root, const std::string& username, std::shared_ptr<Directory>& dir)
{
	if (dir->getCheckNotRemovedFlag() == false) {
		std::string path = dir->getName();
		if (dir->getPath().find_first_of("/") != std::string::npos) {
			path = dir->getPath().substr(dir->getPath().find_first_of("/") + 1, dir->getPath().length());
		}

		std::cout << dir->getPath() << std::endl;
		root->remove(path); // rimuove la root dal path
		std::cout << "eliminato: " << path << std::endl;
		boost::filesystem::path p(username + "/" + root->getName() + "/" + path);
		if (boost::filesystem::exists(p))
			boost::filesystem::remove_all(p);
	}
	else {
		auto dir_children = dir->getChildren(); // boh
		for (auto it = dir_children.begin(); it != dir_children.end(); ++it) {
			if (it->second->type() == 0) { // e' dir
				std::shared_ptr<Directory> sdir = std::dynamic_pointer_cast<Directory>(it->second);
				removeNotFlaggedElements(root, username, sdir);
			}
			else { // e' file
				if (it->second->getCheckNotRemovedFlag() == false) {
					std::string path = it->second->getName();
					if (it->second->getPath().find_first_of("/") != std::string::npos) {
						path = it->second->getPath().substr(it->second->getPath().find_first_of("/") + 1, it->second->getPath().length());
					}

					std::cout << path << std::endl;
					root->remove(path); // rimuove la root dal path
					std::cout << "eliminato: " << path << std::endl;
					boost::filesystem::path p(username + "/" + root->getName() + "/" + path);
					if(boost::filesystem::exists(p))
						boost::filesystem::remove(p);
				}
			}
		}
	}
}

void mkDir(tcp::socket& socket, std::shared_ptr<Directory>& root, const std::string& username, std::istream& input_request_stream)
{
	std::string path_name;
	input_request_stream >> path_name;
	std::replace(path_name.begin(), path_name.end(), '?', ' ');

	std::cout << "mk dir: " << path_name << std::endl;

	boost::filesystem::path p(username + "/" + root->getName() + "/" + path_name);
	boost::filesystem::create_directory(p);

	std::shared_ptr<Directory> ptr = root->addDirectory(path_name);
	if (!ptr) {
		throw std::exception("mkdir - errore in creazione dir");
	}
}

void rmvEl(tcp::socket& socket, std::shared_ptr<Directory>& root, const std::string& username, std::istream& input_request_stream)
{
	std::string path_name;
	input_request_stream >> path_name;
	std::replace(path_name.begin(), path_name.end(), '?', ' ');

	std::cout << "remove el: " << path_name << std::endl;

	boost::filesystem::path p(username + "/" + root->getName() + "/" + path_name);
	
	if (boost::filesystem::exists(p)) {
		boost::filesystem::remove_all(p);

		if (!root->remove(path_name)) {
			throw std::exception("rmv - percorso sbagliato o file inesistente");
		}
	}
	else {
		throw std::exception("rmv - file inesistente");
	}
}

void rnmEl(tcp::socket& socket, std::shared_ptr<Directory>& root, const std::string& username, std::istream& input_request_stream)
{
	std::string path_old_name;
	std::string path_new_name;
	input_request_stream >> path_old_name;
	std::replace(path_old_name.begin(), path_old_name.end(), '?', ' ');
	input_request_stream >> path_new_name;
	std::replace(path_new_name.begin(), path_new_name.end(), '?', ' ');

	std::cout << "rename el: " << path_old_name << " - " << path_new_name << std::endl;

	boost::filesystem::path p_old(username + "/" + root->getName() + "/" + path_old_name);
	boost::filesystem::path p_new(username + "/" + root->getName() + "/" + path_new_name);

	if (boost::filesystem::exists(p_old)) {
		boost::filesystem::rename(p_old, p_new);

		if (!root->rename(path_old_name, path_new_name)) {
			throw std::exception("rnm - percorso sbagliato o file inesistente");
		}
	}
	else {
		throw std::exception("rnm - file inesistente");
	}
}

void startSendingFile(tcp::socket& socket, std::shared_ptr<Directory>& root, const std::string& username, std::istream& input_request_stream)
{
	// Inizializzazione variabili
	boost::array<char, 1024 * 64> buf;
	std::string file_path;
	size_t file_size = -1;
	size_t received_file_size = 0;
	time_t last_edit; // TODO: AGGIORNARE ROOT CON NUOVO FILE
	boost::system::error_code error;

	// ricezione path file e dimensione
	input_request_stream >> file_path;
	std::replace(file_path.begin(), file_path.end(), '?', ' '); // gli spazi mi arrivano come punti interrogativi
	input_request_stream >> file_size;
	input_request_stream >> last_edit;
	input_request_stream.read(buf.c_array(), 2); // eat the "\n\n"

	std::cout << file_path << " size is " << file_size << ", last edited: " << last_edit << std::endl;

	// Invio ACK
	boost::asio::streambuf request_out;
	std::ostream request_stream_out(&request_out);
	request_stream_out << OK << "\n\n";
	boost::asio::write(socket, request_out); // gestire errori

	std::ofstream output_file(username + "/" + root->getName() + "/" + file_path.c_str(), std::ios_base::binary);
	if (!output_file) {
		throw std::exception("rcv file - failed to open file");
	}

	while (true) {
		// Inizializzazione dei buffer necessari per ricevere il messaggio di trasmissione file intermedio
		// (SENDING_FILE o END_SEND_FILE)
		boost::asio::streambuf request_buf;
		boost::asio::read_until(socket, request_buf, "\n\n", error);
		if (error) {
			throw error;
		}
		std::cout << "request size: " << request_buf.size() << "\n";
		std::istream input_request_stream(&request_buf);
		int com_code;
		size_t expected_chunk_size;
		input_request_stream >> com_code;
		input_request_stream >> expected_chunk_size;
		input_request_stream.read(buf.c_array(), 2); // eat the "\n\n"
		std::cout << "rcv file com code: " << com_code << std::endl;
		std::cout << "expected chunk size: " << expected_chunk_size << std::endl;

		if (com_code == SENDING_FILE) {
			ACK(socket);

			// Legge bytes trasmessi e li scrive su file
			size_t len = boost::asio::read(socket, boost::asio::buffer(buf, expected_chunk_size), error);
			ACK(socket);

			received_file_size += len;
			std::cout << "received chunk bytes: " << len << " | progress: " << (received_file_size * 100 / file_size ) << "%" << std::endl;
			if (len > 0) {
				output_file.write(buf.c_array(), (std::streamsize) len);
			}
			if (error) {
				throw error;
			}
		}
		else if (com_code == END_SEND_FILE) {
			// A fine trasmissione, controlla la dimensione file per capire se qualcosa è cambiato durante
			// il trasferimento, in fine chiude il file
			if (output_file.tellp() == (std::fstream::pos_type)(std::streamsize) file_size) {
				ACK(socket);
				std::cout << "file ricevuto senza problemi" << std::endl;
			}
			else {
				sendNotOK(socket);
				throw std::exception("rcv file - dimensione file è cambiata durante la trasmissione, o errore trasmissione file_size");
			}

			//se il file è arrivato si aggiorna il filesystem
			root->remove(file_path); // se c'era già, lo rimuove
			std::shared_ptr<File> ptr = root->addFile(file_path, file_size, last_edit);
			if (!ptr) {
				sendNotOK(socket);
				throw std::exception("rcv file - il file non è stato creato nell'immagine");
			}

			//chiusura file
			std::cout << "received " << output_file.tellp() << " bytes.\n";
			output_file.close();

			boost::filesystem::path p(username + "/" + root->getName() + "/" + file_path.c_str());
			boost::filesystem::last_write_time(p, last_edit);
			break;
		}
	}
}



void clientHandler(tcp::socket& socket)
{
	bool quit = false;
	std::shared_ptr<Directory> root;
	std::string user = "";

	while (!quit) {
		boost::array<char, 1024> buf;
		std::cout << "waiting for message" << std::endl;

		boost::asio::streambuf request_buf;
		boost::system::error_code error;
		boost::asio::read_until(socket, request_buf, "\n\n", error);
		if (error) {
			std::cout << "shutting down client. Reason: " << error.message() << std::endl;
			quit = true;
		}
		else {
			try {
				std::cout << "request size: " << request_buf.size() << "\n";
				std::istream request_stream(&request_buf);
				int com_code;
				request_stream >> com_code;

				std::cout << "com_code: " << com_code << std::endl;

				switch (com_code) {
				case START_COMMUNICATION:
					std::cout << "start communication" << std::endl;
					startCommunication(socket, root, user, request_stream);
					break;

				case VERIFY_CHECKSUM:
					std::cout << "verify checksum" << std::endl;
					verifyChecksum(socket, root, request_stream);
					break;

				case START_SYNC:
					std::cout << "sync started" << std::endl;
					setNotRemovedFlagsRecursive(root, false); // resetta i flag a potenzialmente eliminati
					ACK(socket);
					break;

				case END_SYNC:
					std::cout << "sync ended" << std::endl;
					removeNotFlaggedElements(root, user, root); // elimina tutti gli elementi non flaggati
					root->ls(0);
					break;

				case MK_DIR:
					mkDir(socket, root, user, request_stream);
					ACK(socket);
					root->calculateChecksum();
					root->ls(0);
					break;

				case RMV_ELEMENT:
					rmvEl(socket, root, user, request_stream);
					ACK(socket);
					root->calculateChecksum();
					root->ls(0);
					break;

				case RNM_ELEMENT:
					rnmEl(socket, root, user, request_stream);
					ACK(socket);
					root->calculateChecksum();
					root->ls(0);
					break;

				case START_SEND_FILE:
					startSendingFile(socket, root, user, request_stream);
					root->calculateChecksum();
					root->ls(0);
					break;

				default:
					std::cout << "unexpected com code: " << com_code << std::endl;
					break;
				}

				request_stream.read(buf.c_array(), 2); // eat the "\n\n"
			}
			catch (std::exception & e) {
				std::cout << "EXCEPTION: " << e.what() << std::endl;
				quit = true;
				try {
					sendNotOK(socket);
				}
				catch (boost::system::error_code & error) {
					std::cout << "EXCEPTION: connessione con il client interrotta" << std::endl;
				}
			}
			catch (boost::system::error_code & error) {
				std::cout << "EXCEPTION (ERROR): " << error.message() << std::endl;
				quit = true;
				try {
					sendNotOK(socket);
				}
				catch (boost::system::error_code & error) {
					std::cout << "EXCEPTION: connessione con il client interrotta" << std::endl;
				}
			}
		}
	}
	
	std::lock_guard<std::mutex> lg(m_db);
	if(user != "")
		logged_users.erase(std::find(logged_users.begin(), logged_users.end(), user));
	socket.close();
	std::cout << "end communication with client " << user << std::endl;
}

int main()
{
	std::cout << " # server v. " << PROTOCOL_VERSION << " # \n" << std::endl;

	std::ifstream config_file("config.txt");
	if (!config_file.is_open()) {
		std::cerr << "failed to open config file" << std::endl;
		std::system("pause");
		return -1;
	}

	int n_threads_in_threadpool;
	unsigned short tcp_port;
	config_file >> n_threads_in_threadpool;
	config_file >> tcp_port;

	boost::asio::thread_pool pool(n_threads_in_threadpool);
	std::cout << "threads in thread_pool: " << n_threads_in_threadpool << std::endl;
	
	try {
		std::cout << "server listening on port: " << tcp_port << std::endl;

		boost::asio::io_service io_service;
		boost::asio::ip::tcp::acceptor acceptor(io_service, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), tcp_port));

		boost::system::error_code error;
		
		while (true) {
			boost::asio::ip::tcp::socket socket(io_service);
			acceptor.accept(socket, error);

			if (error) {
				throw error;
			}
			else {
				std::cout << "got client connection." << std::endl;
				boost::asio::post(pool, std::bind(clientHandler, std::move(socket)));
			}
		}
	}
	catch (std::exception & e) {
		std::cout << "EXCEPTION MAIN: " << e.what() << std::endl;
	}
	catch (boost::system::error_code& error) {
		std::cout << "EXCEPTION (ERROR) MAIN: " << error.message() << std::endl;
	}

	pool.join();
	std::system("pause");
	return 0;
}
