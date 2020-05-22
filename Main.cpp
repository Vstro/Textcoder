#include <iostream>
#include <fstream>
#include <windows.h>
#include <time.h>
#include "coders.h"
using namespace std;

int main() {
	SetConsoleOutputCP(1251); // Установка кодовой страницы win-cp 1251 в поток вывода
	SetConsoleCP(1251);
	char inFileName[256], outFileName[256];
	short encoding = 0, choice;
	do {
		cout << "Какую функцию запустить?"
			<< "\n 2) Декодирование файла"
			<< "\n 1) Кодирование файла"
			<< "\n 0) Выход"
			<< "\n>";
		cin >> choice;
		switch (choice)
		{
		case 0: { break; }
		case 1: {
			cout << "Введите имя файла для кодирования: ";
			cin.ignore();
			cin.getline(inFileName, 256);

			// Проверяем существование файла
			fstream inFile(inFileName, ios::in);
			if (!inFile.is_open()) {
				cout << "Такого файла не найдено: проверьте введённое имя!\n\n";
				break;
			}
			inFile.close();

			encoding = Encoder::guessEncoding(inFileName);
			cout << "Введите имя файла после кодирования: ";
			cin.getline(outFileName, 256);
			cout << "В какой кодировке сохранён кодируемый файл? (Предположительно: " << encoding << ")"
				<< "\n 0. Неизвестно"
				<< "\n 1. ANSI или другая 8-битная"
				<< "\n 2. UTF-8"
				<< "\n 3. UTF-16"
				<< "\n 4. UTF-32"
				<< "\n>";
			do {
				cin >> encoding;
				if ((4 < encoding) || (encoding < 0)) {
					cout << "Некорректный вариант!"
						 << "\n>";
				}
			} while ((4 < encoding) || (encoding < 0));
			Encoder coder(encoding, inFileName, outFileName);
			fstream resFile(outFileName, ios::in);
			if (resFile.is_open()) {
				cout << "Кодирование успешно завершено!\n\n";
				resFile.close();
			}
			else {
				cout << "Ошибка! Что-то пошло не так!\n\n";
			}
			break;
		}
		case 2:
		{
			cout << "Введите имя файла для декодирования: ";
			cin.ignore();
			cin.getline(inFileName, 256);

			// Проверяем существование файла
			fstream inFile(inFileName, ios::in);
			if (!inFile.is_open()) {
				cout << "Такого файла не найдено: проверьте введённое имя!\n\n";
				break;
			}
			inFile.close();

			cout << "Введите имя файла после декодирования: ";
			cin.getline(outFileName, 256);
			Decoder decoder(inFileName, outFileName);
			fstream resFile(outFileName, ios::in);
			if (resFile.is_open()) {
				cout << "Декодирование успешно завершено!\n\n";
				resFile.close();
			}
			else {
				cout << "Ошибка! Что-то пошло не так!\n\n";
			}
			break;
		}
		default: {
			cout << "Некорректный вариант!" 
				 << "\n>";
			break; 
		}
		}
	} while (choice != 0);
	system("pause");
	return 0;
}
