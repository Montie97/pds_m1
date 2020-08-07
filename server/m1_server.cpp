
#include <iostream>
#include <fstream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/array.hpp>
#include <string>
#include <boost/filesystem.hpp>
#include "DirectoryAndFilesAnalyzers/Directory.h"
#include "sha1/sha1.h"
using namespace boost::asio::ip;

unsigned short tcp_port = 1234;
enum CommunicationCodes { START_COMMUNICATION, END_COMMUNICATION, VERIFY_CHECKSUM, OK, NOT_OK, MISSING_ELEMENT, /*START_SEND_DIR, END_SEND_DIR,*/ MK_DIR, RMV_ELEMENT, RNM_ELEMENT, START_SEND_FILE, SENDING_FILE, END_SEND_FILE };


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

std::shared_ptr<Directory> build_dir_wrap(boost::filesystem::path p)
{
	if (boost::filesystem::exists(p)) {
		if (boost::filesystem::is_directory(p)) {
			std::shared_ptr<Directory> root = Directory::getRoot();
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

void startCommunication(tcp::socket& socket, std::shared_ptr<Directory>& root, std::istream& input_request_stream)
{
	std::string root_path_name;
	input_request_stream >> root_path_name;
	boost::filesystem::path p(root_path_name);

	boost::asio::streambuf output_request;
	std::ostream output_request_stream(&output_request);

	if (!boost::filesystem::exists(p)) {
		boost::filesystem::create_directory(p);
		output_request_stream << NOT_OK << "\n\n";
	}
	else {
		output_request_stream << OK << "\n\n";
	}
	root = build_dir_wrap(p);

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
		if (de->getChecksum() == checksum) {
			output_request_stream << NOT_OK << "\n\n";
		}
		else {
			output_request_stream << OK << "\n\n";
		}
	}

	boost::asio::write(socket, output_request);
}

void mkDir(tcp::socket& socket, std::shared_ptr<Directory>& root, std::istream& input_request_stream)
{
	std::string path_name;
	input_request_stream >> path_name;

	boost::filesystem::path p(path_name);
	boost::filesystem::create_directory(p);

	std::shared_ptr<Directory> ptr = root->addDirectory(path_name);
	if (!ptr) {
		out("ECCEZZIONAZZA: per quale motivo la dir non è stata creata?");
	}
}

void rmvEl(tcp::socket& socket, std::shared_ptr<Directory>& root, std::istream& input_request_stream)
{
	std::string path_name;
	input_request_stream >> path_name;

	boost::filesystem::path p(path_name);
	
	if (boost::filesystem::exists(p)) {
		boost::filesystem::remove(p);

		if (!root->remove(path_name)) {
			out("ECCEZZIONE: percorso sbagliato o file inesistente");
		}
	}
	else {
		out("File inesistente");
	}
}

void startSendingFile(tcp::socket& socket, std::shared_ptr<Directory>& root, std::istream& input_request_stream)
{
	// Inizializzazione variabili
	boost::array<char, 1024> buf;
	std::string file_path;
	std::string file_name;
	size_t file_size = -1;
	time_t last_edit; // TODO: AGGIORNARE ROOT CON NUOVO FILE
	boost::system::error_code error;

	// ricezione path file e dimensione
	input_request_stream >> file_path;
	input_request_stream >> file_size;
	input_request_stream >> last_edit;
	input_request_stream.read(buf.c_array(), 2); // eat the "\n\n"


	std::cout << file_path << " size is " << file_size << std::endl;
	size_t pos = file_path.find_last_of("/");
	if (pos != std::string::npos)
		file_name = file_path.substr(pos + 1); // file name in file path

	std::ofstream output_file(file_path.c_str(), std::ios_base::binary);
	if (!output_file) {
		std::cout << "ECCEZIONE: failed to open " << file_path << std::endl;
	}

	while (true) {
		// Inizializzazione dei buffer necessari per ricevere il messaggio di trasmissione file intermedio
		// (SENDING_FILE o END_SEND_FILE)
		boost::asio::streambuf request_buf;
		boost::asio::read_until(socket, request_buf, "\n\n");
		std::cout << "request size:" << request_buf.size() << "\n";
		std::istream input_request_stream(&request_buf);
		int com_code;
		input_request_stream >> com_code;

		if (com_code == SENDING_FILE) {
			// Legge bytes trasmessi e li scrive su file
			size_t len = socket.read_some(boost::asio::buffer(buf), error);

			if (len > 0) {
				output_file.write(buf.c_array(), (std::streamsize) len);
			}
			if (error) {
				std::cout << "error: " << error << std::endl;
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
			
			std::cout << "received " << output_file.tellp() << " bytes.\n";
			output_file.close();
			break;
		}
	}

	std::cout << "received " << output_file.tellp() << " bytes.\n";
}

void clientHandler(tcp::socket& socket)
{
	bool quit = false;

	//std::string folder_path = "../_test_folder";
	//boost::filesystem::path p(folder_path);
	std::shared_ptr<Directory> root;// = build_dir_wrap(p);
	//root->ls(4);
	//out("\n\n");

	while (!quit) {
		boost::array<char, 1024> buf;

		boost::asio::streambuf request_buf;
		boost::asio::read_until(socket, request_buf, "\n\n");
		std::cout << "request size:" << request_buf.size() << "\n";
		std::istream request_stream(&request_buf);
		int com_code;
		request_stream >> com_code;

		switch (com_code) {
			case START_COMMUNICATION:
				startCommunication(socket, root, request_stream);
				break;
			
			case VERIFY_CHECKSUM:
				verifyChecksum(socket, root, request_stream);
				break;

			case MK_DIR:
				mkDir(socket, root, request_stream);
				break;

			case RMV_ELEMENT:
				rmvEl(socket, root, request_stream);
				break;

			case RNM_ELEMENT:
				break;

			case START_SEND_FILE:
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

int main()
{
	std::string folder_path = "../_test_folder";
	boost::filesystem::path p(folder_path);
	std::shared_ptr<Directory> root = build_dir_wrap(p);
	root->ls(0);
	return 5;


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
