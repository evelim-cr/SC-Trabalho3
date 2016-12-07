#ifndef _BRUTEXOR_H_
#define _BRUTEXOR_H_

#define REG_HEX "^[a-f]$|^[A-F]$"

//http://stackoverflow.com/a/8389763
//gcc brutexor.c -o brutextor -lcrypto -lssl

// Retorna o char correspondende ao valor passado, utilizando os valores ASCII
char retriveChar(int value);

// Flag para dizer se o caracter deve ser gerado ou não
int return_flag(int value);

// Retorna o char para os caracteres de 1 a n-1 para uma string de n caracteres
char return_char(int value,int * size);

// Concatena um caracter a uma string
void append_char_function(char append_char, char *word, int *i);

#endif
