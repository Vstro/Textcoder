#pragma once
#include <fstream>
#include <time.h>
using namespace std;

const unsigned int FULL_UNICODE_ALPHABET_LENGTH = 0x10F800; // Количество всех символов Юникода
const unsigned short FULL_ANSI_ALPHABET_LENGTH = 0x100; // Количество всех однобайтовых символов
typedef char Char32[4]; // Новое имя для 4-байтового массива символов
enum encodings { UNKNOWN, ANSI, UTF8, UTF16, UTF32 }; // Обозначения для наиболее используемых текстовых кодировок

class Coder { // Содержит общие элементы классов Encoder и Decoder
protected:
	Char32* alphabet32; // Указатель на алфавит из 4-байтовых последовательностей
	char* alphabet; // Указатель на алфавит из байтов
	unsigned int alphabetLength = 0; // Длина алфавита
	int bitsForChar; // Количество битов, кодирующих один символ
	bool* bitTextBuffer; // Буфер для битов
	unsigned short remainingBits = 0; // Количество значащих битов оставшихся в bitTextBuffer

	// Получает первый байт из байтовой последовательности обозначающей символ в UTF-8
	// Возвращает оставшееся число байтов последовательности (0-3) или -1, если байт имеет некорректное значение
	int remainBytesForCharUTF8(char c);

	// Получает первые 2 байта из байтовой последовательности обозначающей символ в UTF-16
	// Возвращает true, если эти 2 байта - завершённый символ UTF-16, и false, если необходимы ещё 2 байта
	bool isFullCharUTF16(unsigned char c1, unsigned char c2, bool UTF16LE);
};

class Encoder : public Coder {
public:
	// Конструктор, вызывает функцию кодирования
	Encoder(short inFileEncoding, const char* inFileName, const char* outFileName = "coded.txt") {
		encode(inFileEncoding, inFileName, outFileName);
	}

	// Проверяет первые байты файла fileName на наличие характерных меток кодировок
	// Возвращает encodings соответствующий предположительной кодировке
	static encodings guessEncoding(const char* fileName);

private:
	// Кодирует inFileName (в кодировке inFileEncoding) в outFileName
	void encode(short inFileEncoding, const char* inFileName, const char* outFileName);

	// Возвращает номер символа в алфавите, если он был найден или успешно добавлен (иначе -1)
	int checkAlphabet32(Char32 c);

	// Возвращает номер символа в алфавите, если он был найден или успешно добавлен
	int checkAlphabet(char c);

	// Перемешивает алфавит из 4-байтовых последовательностей случайным образом, чтобы очерёдность символов
	// в алфавите (а значит и номера их кодирующие) не совпадали с очерёдностью символов в кодируемом файле
	void shuffleAlphabet32();

	// Перемешивает алфавит из байтов случайным образом
	void shuffleAlphabet();

	// Помещает BitsForChar младших битов newBits в конец bitTextBuffer
	void putBitsForCharInBuffer(int newBits);

	// Получает макс. размер алфавита, под который был выделен bitTextBuffer (нужно для определения его рамера)
	// Возвращает байт, образованный первыми 8 битами bitTextBuffer
	char getByteFromBuffer(unsigned int fullAlphabetLength);

	// Получает макс. размер алфавита, под который был выделен bitTextBuffer (нужно для определения его рамера)
	// Заполняет bitTextBuffer нулями
	void clearBitTextBuffer(unsigned int fullAlphabetLength);
};

class Decoder : public Coder {
public:
	// Конструктор, вызывает функцию декодирования
	Decoder(const char* inFileName, const char* outFileName = "decoded.txt") {
		decode(inFileName, outFileName);
	}

private:
	// Декодирует inFileName в outFileName
	void decode(const char* inFileName, const char* outFileName);

	// Помещает байт в конец bitTextBuffer
	void putByteInBuffer(int byte);

	// Получает макс. размер алфавита, под который был выделен bitTextBuffer (нужно для определения его рамера)
	// Возвращает беззнаковое целое, образованное первыми BitsForChar битами bitTextBuffer
	unsigned int getBitsForCharFromBuffer(unsigned int fullAlphabetLength);
};
