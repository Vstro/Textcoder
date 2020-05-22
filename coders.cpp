#include "coders.h"
// Реализация функций классов Coder, Encoder и Decoder

int Coder::remainBytesForCharUTF8(char c) {
	if (!((int)c & 0b10000000)) return 0;
	for (int i = 2; i < 5; i++) {
		if (!((int)c & (0b10000000 >> i))) return (i - 1);
	}
	return -1;
}

bool Coder::isFullCharUTF16(unsigned char c1, unsigned char c2, bool UTF16LE) {
	int word;
	if (UTF16LE) {
		word = ((int)c2 << 8) + (int)c1;
	}
	else {
		word = ((int)c1 << 8) + (int)c2;
	}
	if ((word < 0xD800) || (word > 0xDFFF)) return true;
	return false;
}

encodings Encoder::guessEncoding(const char* fileName) {
	fstream File(fileName, ios::in | ios::binary);
	Char32 char32Buffer = {};
	for (int i = 0; i < 3; i++) {
		char32Buffer[i] = (File.eof()) ? (0) : ((char)File.get());
	}
	File.close();
	if ((char32Buffer[0] == -17) && (char32Buffer[1] == -69) && (char32Buffer[2] == -65)) return UTF8;
	if ((char32Buffer[0] == -1) && (char32Buffer[1] == -2)) {
		if (char32Buffer[2] == 0) return UTF32;
		return UTF16;
	}
	if ((char32Buffer[0] == -2) && (char32Buffer[1] == -1)) {
		return UTF16;
	}
	if ((char32Buffer[0] == 0) && (char32Buffer[1] == 0) && (char32Buffer[2] == -2)) {
		return UTF32;
	}
	return ANSI;
}

void Encoder::encode(short inFileEncoding, const char* inFileName, const char* outFileName) {
	fstream inFile(inFileName, ios::in | ios::binary);
	if (!inFile.is_open()) return; // Выходим из кодировщика так и не создав закодированный файл -> признак ошибки

	switch (inFileEncoding) {
	case ANSI: {
		alphabet = new char[FULL_ANSI_ALPHABET_LENGTH];
		bitTextBuffer = new bool[(int)(2 * ceil(log2(FULL_ANSI_ALPHABET_LENGTH)))];
		clearBitTextBuffer(FULL_ANSI_ALPHABET_LENGTH);

		// Формирование конкретного алфавита
		char charBuffer;
		charBuffer = (char)inFile.get();
		while (!inFile.eof()) {
			checkAlphabet(charBuffer);
			charBuffer = (char)inFile.get();
		}

		//Перемешивание Alphabet
		shuffleAlphabet();

		// Записываем метку изначальной кодировки и длину алфавита для конкретного файла в 2 байта (1+1)
		fstream outFile(outFileName, ios::trunc | ios::out | ios::binary);
		if (alphabetLength == 0) { // Если кодируемый файл абсолютно пуст, то выходим, создав аналогичный
			outFile.close();
			break;
		}
		outFile.put((char)inFileEncoding);
		outFile.put((char)alphabetLength);

		// Записываем изначальный алфавит для возможности обратного декодирования
		for (unsigned int i = 0; i < alphabetLength; i++) {
			outFile.put(alphabet[i]);
		}

		if (alphabetLength == 1) {
			bitsForChar = 1;
		}
		else {
			bitsForChar = (int)ceil(log2(alphabetLength));
		}
		inFile.close();
		inFile.open(inFileName, ios::in | ios::binary); // Возвращаемся к началу файла

		// Второй проход по файлу -> Кодируем
		while (!inFile.eof()) {
			while ((remainingBits < 8) && (!inFile.eof())) {
				charBuffer = (char)inFile.get();
				if (inFile.eof()) {
					break;
				}
				putBitsForCharInBuffer(checkAlphabet(charBuffer));
			}
			while (remainingBits >= 8) {
				outFile.put(getByteFromBuffer(FULL_ANSI_ALPHABET_LENGTH));
			}
		}
		if (remainingBits > 0) { // Если в буфере ещё остались биты (они не записались в основном цикле т.к. их меньше 8)
			outFile.put(getByteFromBuffer(FULL_ANSI_ALPHABET_LENGTH)); // Записываем последние биты (до целого байта их дополняют нули)
		}
		delete[] alphabet;
		delete[] bitTextBuffer;
		outFile.close();
		break;
	}
	case UTF8: {
		alphabet32 = new Char32[FULL_UNICODE_ALPHABET_LENGTH];
		bitTextBuffer = new bool[(int)(2 * ceil(log2(FULL_UNICODE_ALPHABET_LENGTH)))];
		clearBitTextBuffer(FULL_UNICODE_ALPHABET_LENGTH);

		// Формирование конкретного алфавита
		Char32 char32Buffer;
		char32Buffer[0] = (char)inFile.get(); // Читаем первый байт последовательности
		short remainingBytes = remainBytesForCharUTF8(char32Buffer[0]); // Определяем сколько ещё байтов нужно прочесть до конца данного символа
		for (int i = 1; i < 4; i++) { // Заполняем оставшиеся 3 байта char32Buffer либо прочитанными байтами либо 0
			if (i <= remainingBytes) {
				char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
			}
			else {
				char32Buffer[i] = 0;
			}
		}
		while (!inFile.eof()) {
			if (checkAlphabet32(char32Buffer) == -1) break;
			char32Buffer[0] = (char)inFile.get(); // Читаем первый байт последовательности
			remainingBytes = remainBytesForCharUTF8(char32Buffer[0]); // Определяем сколько ещё байтов нужно прочесть до конца данного символа
			for (int i = 1; i < 4; i++) { // Заполняем оставшиеся 3 байта char32Buffer либо прочитанными байтами либо 0
				if (i <= remainingBytes) {
					char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
				}
				else {
					char32Buffer[i] = 0;
				}
			}
		}
		if (alphabetLength == FULL_UNICODE_ALPHABET_LENGTH) {
			break; // Выходим из кодировщика так и не создав закодированный файл -> признак ошибки
		}

		//Перемешивание Alphabet32
		shuffleAlphabet32();

		// Записываем метку изначальной кодировки и длину алфавита для конкретного файла в 1 и 3 байта соответственно
		fstream outFile(outFileName, ios::trunc | ios::out | ios::binary);
		if (alphabetLength == 0) { // Если кодируемый файл абсолютно пуст, то выходим, создав аналогичный
			outFile.close();
			break;
		}
		outFile.put((char)inFileEncoding);
		for (int i = 2; i >= 0; i--) {
			char byte = (alphabetLength >> (8 * i)) % 256;
			outFile.put(byte);
		}

		// Записываем изначальный алфавит для возможности обратного декодирования
		for (unsigned int i = 0; i < alphabetLength; i++) {
			remainingBytes = remainBytesForCharUTF8(alphabet32[i][0]);
			for (int j = 0; j < remainingBytes + 1; j++) {
				outFile.put(alphabet32[i][j]);
			}
		}

		if (alphabetLength == 1) {
			bitsForChar = 1;
		}
		else {
			bitsForChar = (int)ceil(log2(alphabetLength));
		}
		inFile.close();
		inFile.open(inFileName, ios::in | ios::binary); // Возвращаемся к началу файла

		// Второй проход по файлу -> Кодируем
		while (!inFile.eof()) {
			while ((remainingBits < 8) && (!inFile.eof())) {
				char32Buffer[0] = (char)inFile.get(); // Читаем первый байт последовательности
				remainingBytes = remainBytesForCharUTF8(char32Buffer[0]); // Определяем сколько ещё байтов нужно прочесть до конца данного символа
				for (int i = 1; i < 4; i++) { // Заполняем оставшиеся 3 байта char32Buffer либо прочитанными байтами либо 0
					if (i <= remainingBytes) {
						char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
					}
					else {
						char32Buffer[i] = 0;
					}
				}
				if (inFile.eof()) {
					break;
				}
				putBitsForCharInBuffer(checkAlphabet32(char32Buffer));
			}
			while (remainingBits >= 8) {
				outFile.put(getByteFromBuffer(FULL_UNICODE_ALPHABET_LENGTH));
			}
		}
		if (remainingBits > 0) { // Если в буфере ещё остались биты (они не записались в основном цикле т.к. их меньше 8)
			outFile.put(getByteFromBuffer(FULL_UNICODE_ALPHABET_LENGTH)); // Записываем последние биты (до целого байта их дополняют нули)
		}
		delete[] alphabet32;
		delete[] bitTextBuffer;
		outFile.close();
		break;
	}
	case UTF16: {
		alphabet32 = new Char32[FULL_UNICODE_ALPHABET_LENGTH];
		bitTextBuffer = new bool[(int)(2 * ceil(log2(FULL_UNICODE_ALPHABET_LENGTH)))];
		bool UTF16LE = false; // Метка Little Endian или Big Endian (при LE очерёдность байтов в словах обратная)
		clearBitTextBuffer(FULL_UNICODE_ALPHABET_LENGTH);

		// Формирование конкретного алфавита
		Char32 char32Buffer;
		for (int i = 0; i < 2; i++) { // Читаем первое слово последовательности
			char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
		}
		if (!isFullCharUTF16(char32Buffer[0], char32Buffer[1], UTF16LE)) { // Заполняем оставшиеся 2 байта char32Buffer либо прочитанными байтами либо 0
			for (int i = 2; i < 4; i++) {
				char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
			}
		}
		else {
			for (int i = 2; i < 4; i++) {
				char32Buffer[i] = 0;
			}
		}
		if (((int)char32Buffer[0] == -1) && ((int)char32Buffer[1] == -2)) UTF16LE = true;
		while (!inFile.eof()) {
			if (checkAlphabet32(char32Buffer) == -1) break;
			for (int i = 0; i < 2; i++) { // Читаем первое слово последовательности
				char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
			}
			if (!isFullCharUTF16(char32Buffer[0], char32Buffer[1], UTF16LE)) { // Заполняем оставшиеся 2 байта char32Buffer либо прочитанными байтами либо 0
				for (int i = 2; i < 4; i++) {
					char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
				}
			}
			else {
				for (int i = 2; i < 4; i++) {
					char32Buffer[i] = 0;
				}
			}
		}
		if (alphabetLength == FULL_UNICODE_ALPHABET_LENGTH) {
			break; // Выходим из кодировщика так и не создав закодированный файл -> признак ошибки
		}

		//Перемешивание Alphabet32
		shuffleAlphabet32();

		// Записываем метку изначальной кодировки и длину алфавита для конкретного файла в 1 и 3 байта соответственно
		fstream outFile(outFileName, ios::trunc | ios::out | ios::binary);
		if (alphabetLength == 0) { // Если кодируемый файл абсолютно пуст, то выходим, создав аналогичный
			outFile.close();
			break;
		}
		outFile.put((char)(inFileEncoding + (UTF16LE << 4)));
		for (int i = 2; i >= 0; i--) {
			char byte = (alphabetLength >> (8 * i)) % 256;
			outFile.put(byte);
		}

		// Записываем изначальный алфавит для возможности обратного декодирования
		for (unsigned int i = 0; i < alphabetLength; i++) {
			if (isFullCharUTF16(alphabet32[i][0], alphabet32[i][1], UTF16LE)) {
				for (int j = 0; j < 2; j++) {
					outFile.put(alphabet32[i][j]);
				}
			}
			else {
				for (int j = 0; j < 4; j++) {
					outFile.put(alphabet32[i][j]);
				}
			}
		}

		if (alphabetLength == 1) {
			bitsForChar = 1;
		}
		else {
			bitsForChar = (int)ceil(log2(alphabetLength));
		}
		inFile.close();
		inFile.open(inFileName, ios::in | ios::binary); // Возвращаемся к началу файла

		// Второй проход по файлу -> Кодируем
		while (!inFile.eof()) {
			while ((remainingBits < 8) && (!inFile.eof())) {
				for (int i = 0; i < 2; i++) { // Читаем первое слово последовательности
					char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
				}
				if (!isFullCharUTF16(char32Buffer[0], char32Buffer[1], UTF16LE)) { // Заполняем оставшиеся 2 байта char32Buffer либо прочитанными байтами либо 0
					for (int i = 2; i < 4; i++) {
						char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
					}
				}
				else {
					for (int i = 2; i < 4; i++) {
						char32Buffer[i] = 0;
					}
				}
				if (inFile.eof()) {
					break;
				}
				putBitsForCharInBuffer(checkAlphabet32(char32Buffer));
			}
			while (remainingBits >= 8) {
				outFile.put(getByteFromBuffer(FULL_UNICODE_ALPHABET_LENGTH));
			}
		}
		if (remainingBits > 0) { // Если в буфере ещё остались биты (они не записались в основном цикле т.к. их меньше 8)
			outFile.put(getByteFromBuffer(FULL_UNICODE_ALPHABET_LENGTH)); // Записываем последние биты (до целого байта их дополняют нули)
		}
		delete[] alphabet32;
		delete[] bitTextBuffer;
		outFile.close();
		break;
	}
	case UNKNOWN: case UTF32: {
		alphabet32 = new Char32[FULL_UNICODE_ALPHABET_LENGTH];
		bitTextBuffer = new bool[(int)(2 * ceil(log2(FULL_UNICODE_ALPHABET_LENGTH)))];
		clearBitTextBuffer(FULL_UNICODE_ALPHABET_LENGTH);

		// Формирование конкретного алфавита
		Char32 char32Buffer;
		for (int i = 0; i < 4; i++) { // Чтение по 4 байта
			char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
		}
		while (!inFile.eof()) {
			if (checkAlphabet32(char32Buffer) == -1) break; // Если нарушается соответствие 1 символ = 4 байта, то кол-во возможных 4-байтовых последовательностей может превысить число символов Юникода, а значит размер массива алфавита
			for (int i = 0; i < 4; i++) {
				char32Buffer[i] = (inFile.eof()) ? (0) : ((char)inFile.get());
			}
		}
		if (alphabetLength == FULL_UNICODE_ALPHABET_LENGTH) {
			break; // Выходим из кодировщика так и не создав закодированный файл -> признак ошибки
		}
		for (int i = 3; i >= 0; i--) {
			if (char32Buffer[i] == -1) { // inFile.get() вернул -1, когда считывать было больше нечего
				if (i != 0) {
					char32Buffer[i] = 0;
					checkAlphabet32(char32Buffer); // Отправляем в алфавит последние байты (среди них будут значащие, если кол-во байтов inFile не кратно 4)
				}
				break;
			}
		}

		//Перемешивание Alphabet32
		shuffleAlphabet32();

		// Записываем метку изначальной кодировки и длину алфавита для конкретного файла в 1 и 3 байта соответственно
		fstream outFile(outFileName, ios::trunc | ios::out | ios::binary);
		if (alphabetLength == 0) { // Если кодируемый файл абсолютно пуст, то выходим, создав аналогичный
			outFile.close();
			break;
		}
		outFile.put((char)inFileEncoding);
		for (int i = 2; i >= 0; i--) {
			char byte = (alphabetLength >> (8 * i)) % 256;
			outFile.put(byte);
		}

		// Записываем изначальный алфавит для возможности обратного декодирования
		for (unsigned int i = 0; i < alphabetLength; i++) {
			for (int j = 0; j < 4; j++) {
				outFile.put(alphabet32[i][j]);
			}
		}

		if (alphabetLength == 1) {
			bitsForChar = 1;
		}
		else {
			bitsForChar = (int)ceil(log2(alphabetLength));
		}
		inFile.close();
		inFile.open(inFileName, ios::in | ios::binary); // Возвращаемся к началу файла

		// Второй проход по файлу -> Кодируем
		while (!inFile.eof()) {
			while ((remainingBits < 8) && (!inFile.eof())) {
				for (int j = 0; j < 4; j++) {
					char32Buffer[j] = (inFile.eof()) ? (0) : ((char)inFile.get());
				}
				if (inFile.eof()) {
					int i;
					for (i = 3; i >= 0; i--) {
						if (char32Buffer[i] == -1) { // inFile.get() вернул -1, когда считывать было больше нечего
							char32Buffer[i] = 0;
							break; // i сохранит номер байта в char32Buffer, который был -1
						}
					}
					if (i == 0) break; // Если последняя последовательность из 4 байтов начиналась с -1, то в ней нет значащих байтов
				}
				putBitsForCharInBuffer(checkAlphabet32(char32Buffer));
			}
			while (remainingBits >= 8) {
				outFile.put(getByteFromBuffer(FULL_UNICODE_ALPHABET_LENGTH));
			}
		}
		if (remainingBits > 0) { // Если в буфере ещё остались биты (они не записались в основном цикле т.к. их меньше 8)
			outFile.put(getByteFromBuffer(FULL_UNICODE_ALPHABET_LENGTH)); // Записываем последние биты (до целого байта их дополняют нули)
		}
		delete[] alphabet32;
		delete[] bitTextBuffer;
		outFile.close();
		break;
	}
	}
	inFile.close();
}

int Encoder::checkAlphabet32(Char32 c) {
	for (unsigned int i = 0; i < alphabetLength; i++) {
		if ((alphabet32[i][0] == c[0]) && (alphabet32[i][1] == c[1]) &&
			(alphabet32[i][2] == c[2]) && (alphabet32[i][3] == c[3])) return i;
	}
	if (alphabetLength == FULL_UNICODE_ALPHABET_LENGTH) return -1;
	for (int i = 0; i < 4; i++) {
		alphabet32[alphabetLength][i] = c[i];
	}
	alphabetLength++;
	return (alphabetLength - 1);
}

int Encoder::checkAlphabet(char c) {
	for (unsigned int i = 0; i < alphabetLength; i++) {
		if (alphabet[i] == c) return i;
	}
	alphabet[alphabetLength] = c;
	alphabetLength++;
	return (alphabetLength - 1);
}

void Encoder::shuffleAlphabet32() {
	srand((unsigned int)time(NULL));
	for (unsigned int i = 0; i < alphabetLength; i++) {
		swap(alphabet32[i], alphabet32[rand() % alphabetLength]);
	}
}

void Encoder::shuffleAlphabet() {
	srand((unsigned int)time(NULL));
	for (unsigned int i = 0; i < alphabetLength; i++) {
		swap(alphabet[i], alphabet[rand() % alphabetLength]);
	}
}

void Encoder::putBitsForCharInBuffer(int newBits) {
	for (int i = 0; i < bitsForChar; i++) {
		bitTextBuffer[remainingBits] = newBits % 2;
		newBits = newBits >> 1;
		remainingBits++;
	}
}

char Encoder::getByteFromBuffer(unsigned int fullAlphabetLength) {
	int buf = 0;
	for (int i = 0; i < 8; i++) {
		buf += bitTextBuffer[i] * (int)pow(2, i);
	}
	for (int i = 0; i < 2 * ceil(log2(fullAlphabetLength)) - 8; i++) {
		bitTextBuffer[i] = bitTextBuffer[i + 8];
	}
	for (int i = 2 * (int)ceil(log2(fullAlphabetLength)) - 8; i < 2 * ceil(log2(fullAlphabetLength)); i++) {
		bitTextBuffer[i] = 0;
	}
	if (remainingBits < 8) remainingBits = 0;
	else remainingBits -= 8;
	return (char)buf;
}

void Encoder::clearBitTextBuffer(unsigned int fullAlphabetLength) {
	for (int i = 0; i < 2 * ceil(log2(fullAlphabetLength)); i++) {
		bitTextBuffer[i] = 0;
	}
}

void Decoder::decode(const char* inFileName, const char* outFileName) {
	fstream inFile(inFileName, ios::in | ios::binary);
	if (!inFile.is_open()) return; // Выходим из декодировщика так и не создав декодированный файл -> признак ошибки

	fstream outFile(outFileName, ios::trunc | ios::out | ios::binary);
	short inFileEncoding = inFile.get(); // Считываем метку изначальной кодировки

	switch (inFileEncoding % 0b10000) { // Используем младшую половину байта (в старшей - метка LE/BE UTF16)
	case ANSI: {
		alphabet = new char[FULL_ANSI_ALPHABET_LENGTH];
		bitTextBuffer = new bool[(int)(2 * ceil(log2(FULL_ANSI_ALPHABET_LENGTH)))];

		// Считываем длину алфавита вычисляем кол-во битов кодирующих символ для конкретного файла
		alphabetLength = inFile.get();
		if (alphabetLength == 1) {
			bitsForChar = 1;
		}
		else {
			bitsForChar = (int)ceil(log2(alphabetLength));
		}

		// Считываем изначальный алфавит
		for (unsigned int i = 0; i < alphabetLength; i++) {
			alphabet[i] = (char)inFile.get();
		}

		// Декодируем
		char charBuffer;
		while (!inFile.eof() && (bitsForChar > 0)) {
			while (remainingBits < bitsForChar) {
				charBuffer = (char)inFile.get();
				if (inFile.eof()) break;
				putByteInBuffer((int)charBuffer);
			}
			while ((remainingBits >= bitsForChar) && (bitsForChar > 0)) {
				outFile.put(alphabet[getBitsForCharFromBuffer(FULL_ANSI_ALPHABET_LENGTH)]);
			}
		}
		delete[] alphabet;
		delete[] bitTextBuffer;
		break;
	}
	case UTF8: {
		alphabet32 = new Char32[FULL_UNICODE_ALPHABET_LENGTH];
		bitTextBuffer = new bool[(int)(2 * ceil(log2(FULL_UNICODE_ALPHABET_LENGTH)))];

		// Считываем длину алфавита, вычисляем кол-во битов кодирующих символ для конкретного файла
		for (int i = 0; i < 3; i++) {
			alphabetLength = alphabetLength << 8;
			alphabetLength += inFile.get();
		}
		if (alphabetLength == 1) {
			bitsForChar = 1;
		}
		else {
			bitsForChar = (int)ceil(log2(alphabetLength));
		}

		// Считываем изначальный алфавит
		for (unsigned int i = 0; i < alphabetLength; i++) {
			alphabet32[i][0] = (char)inFile.get(); // Читаем первый байт последовательности
			short remainingBytes = remainBytesForCharUTF8(alphabet32[i][0]); // Определяем сколько ещё байтов нужно прочесть до конца данного символа
			for (int j = 1; j < 4; j++) { // Заполняем оставшиеся 3 байта char32Buffer либо прочитанными байтами либо 0
				if (j <= remainingBytes) {
					alphabet32[i][j] = (inFile.eof()) ? (0) : ((char)inFile.get());
				}
				else {
					alphabet32[i][j] = 0;
				}
			}
		}

		// Декодируем
		char charBuffer;
		while (!inFile.eof() && (bitsForChar > 0)) {
			while (remainingBits < bitsForChar) {
				charBuffer = (char)inFile.get();
				if (inFile.eof()) break;
				putByteInBuffer((int)charBuffer);
			}
			while ((remainingBits >= bitsForChar) && (bitsForChar > 0)) {
				int charNum = getBitsForCharFromBuffer(FULL_UNICODE_ALPHABET_LENGTH);
				short remainingBytes = remainBytesForCharUTF8(alphabet32[charNum][0]);
				for (int j = 0; j < remainingBytes + 1; j++) {
					outFile.put(alphabet32[charNum][j]);
				}
			}
		}
		delete[] alphabet32;
		delete[] bitTextBuffer;
		break;
	}
	case UTF16: {
		alphabet32 = new Char32[FULL_UNICODE_ALPHABET_LENGTH];
		bitTextBuffer = new bool[(int)(2 * ceil(log2(FULL_UNICODE_ALPHABET_LENGTH)))];
		bool UTF16LE = (inFileEncoding >> 4); // Считываем записанную при кодировании метку LE/BE UTF16

		// Считываем длину алфавита, вычисляем кол-во битов кодирующих символ для конкретного файла
		for (int i = 0; i < 3; i++) {
			alphabetLength = alphabetLength << 8;
			alphabetLength += inFile.get();
		}
		if (alphabetLength == 1) {
			bitsForChar = 1;
		}
		else {
			bitsForChar = (int)ceil(log2(alphabetLength));
		}

		// Считываем изначальный алфавит
		for (unsigned int j = 0; j < alphabetLength; j++) {
			for (int i = 0; i < 2; i++) { // Читаем первое слово последовательности
				alphabet32[j][i] = (inFile.eof()) ? (0) : ((char)inFile.get());
			}
			if (!isFullCharUTF16(alphabet32[j][0], alphabet32[j][1], UTF16LE)) { // Заполняем оставшиеся 2 байта char32Buffer либо прочитанными байтами либо 0
				for (int i = 2; i < 4; i++) {
					alphabet32[j][i] = (inFile.eof()) ? (0) : ((char)inFile.get());
				}
			}
			else {
				for (int i = 2; i < 4; i++) {
					alphabet32[j][i] = 0;
				}
			}
		}

		// Декодируем
		char charBuffer;
		while (!inFile.eof() && (bitsForChar > 0)) {
			while (remainingBits < bitsForChar) {
				charBuffer = (char)inFile.get();
				if (inFile.eof()) break;
				putByteInBuffer((int)charBuffer);
			}
			while ((remainingBits >= bitsForChar) && (bitsForChar > 0)) {
				int charNum = getBitsForCharFromBuffer(FULL_UNICODE_ALPHABET_LENGTH);
				if (isFullCharUTF16(alphabet32[charNum][0], alphabet32[charNum][1], UTF16LE)) {
					for (int j = 0; j < 2; j++) {
						outFile.put(alphabet32[charNum][j]);
					}
				}
				else {
					for (int j = 0; j < 4; j++) {
						outFile.put(alphabet32[charNum][j]);
					}
				}
			}
		}
		delete[] alphabet32;
		delete[] bitTextBuffer;
		break;
	}
	case UNKNOWN: case UTF32: {
		alphabet32 = new Char32[FULL_UNICODE_ALPHABET_LENGTH];
		bitTextBuffer = new bool[(int)(2 * ceil(log2(FULL_UNICODE_ALPHABET_LENGTH)))];

		// Считываем длину алфавита вычисляем кол-во битов кодирующих символ для конкретного файла
		for (int i = 0; i < 3; i++) {
			alphabetLength = alphabetLength << 8;
			alphabetLength += inFile.get();
		}
		if (alphabetLength == 1) {
			bitsForChar = 1;
		}
		else {
			bitsForChar = (int)ceil(log2(alphabetLength));
		}

		// Считываем изначальный алфавит
		for (unsigned int i = 0; i < alphabetLength; i++) {
			for (int j = 0; j < 4; j++) {
				alphabet32[i][j] = (char)inFile.get();
			}
		}

		// Декодируем
		char charBuffer;
		while (!inFile.eof() && (bitsForChar > 0)) {
			while (remainingBits < bitsForChar) {
				charBuffer = (char)inFile.get();
				if (inFile.eof()) break;
				putByteInBuffer((int)charBuffer);
			}
			while ((remainingBits >= bitsForChar) && (bitsForChar > 0)) {
				int charNum = getBitsForCharFromBuffer(FULL_UNICODE_ALPHABET_LENGTH);
				for (int i = 0; i < 4; i++) {
					outFile.put(alphabet32[charNum][i]);
				}
			}
		}
		delete[] alphabet32;
		delete[] bitTextBuffer;
		break;
	}
	}

	inFile.close();
	outFile.close();
}

void Decoder::putByteInBuffer(int byte) {
	for (int i = 0; i < 8; i++) {
		bitTextBuffer[remainingBits] = byte % 2;
		byte = byte >> 1;
		remainingBits++;
	}
}

unsigned int Decoder::getBitsForCharFromBuffer(unsigned int fullAlphabetLength) {
	unsigned int buf = 0;
	for (int i = 0; i < bitsForChar; i++) {
		buf += bitTextBuffer[i] * (int)pow(2, i);
	}
	for (int i = 0; i < 2 * ceil(log2(fullAlphabetLength)) - bitsForChar; i++) {
		bitTextBuffer[i] = bitTextBuffer[i + bitsForChar];
	}
	for (int i = 2 * (int)ceil(log2(fullAlphabetLength)) - bitsForChar; i < 2 * ceil(log2(fullAlphabetLength)); i++) {
		bitTextBuffer[i] = 0;
	}
	remainingBits -= bitsForChar;
	return buf;
}
