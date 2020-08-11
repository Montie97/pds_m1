#ifndef SHA1_HEADER

#include <string>

typedef unsigned int Uint32;

class SHA1
{
private:
	// fields
	Uint32 H0, H1, H2, H3, H4;
	unsigned char bytes[64];
	int unprocessedBytes;
	Uint32 size;
	void process();
public:
	SHA1();
	~SHA1();
	void addBytes(const char* data, int num);
	unsigned char* getDigest();
	// utility methods
	static Uint32 lrot(Uint32 x, int bits);
	static void storeBigEndianUint32(unsigned char* byte, Uint32 num);
	static void hexPrinter(unsigned char* c, int l);
	std::string getDigestToHexString();
};

#define SHA1_HEADER
#endif