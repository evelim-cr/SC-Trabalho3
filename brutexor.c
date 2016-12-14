#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <regex.h>
#include <time.h>
#include <sys/time.h>

#include "brutexor.h"

//http://stackoverflow.com/a/8389763
//gcc brutexor.c -o brutextor -lcrypto -lssl

// Retorna o char correspondende ao valor passado, utilizando os valores ASCII
char retriveChar(int value){
    char character;
    
    // valores de 0-9
    if(value >= 0 && value < 10){
        character = 48 + value;
    }
    // valores de A-Z
    else if(value >= 10 && value < 36){
        character = 65 + (value - 10);
    }
    // valores de a-z
    else if(value >= 36 && value < 62){
        character = 97 + (value - 36);
    }
        
    return character;
}

// Flag para dizer se o caracter deve ser gerado ou não
int return_flag(int value){
    if(value == 0)
        return 0;
    else
        return 1;
}

// Retorna o char para os caracteres de 1 a n-1 para uma string de n caracteres
char return_char(int value,int * size){
    if(value > 0){
        size ++;
        return retriveChar(value - 1);
    }
    return '\0';
}

// Concatena um caracter a uma string
void append_char_function(char append_char, char *word, int *i){
    if(append_char != '\0'){
        word[(*i)] = append_char;
        (*i)++;
    }
}

// Retorna a palavra correspondente ao md5
void findWord() {
    int size = 0;
    int first, second, third, fourth, fifth, sixth, seventh, eighth;
    int second_flag, third_flag, fourth_flag, fifth_flag, sixth_flag, seventh_flag, eighth_flag;
    char first_char, second_char, third_char, fourth_char, fifth_char, sixth_char, seventh_char, eighth_char;
    int word_index = 0;
    char *word;
	char buffer[33];
    
	for(first = 0; first <= 62; first ++){
        first_char = return_char(first, &size);
        
        for(second = return_flag(first); second <= 62; second ++){
            second_char = return_char(second, &size);
            
            for(third = return_flag(second); third <= 62; third ++){
                third_char = return_char(third, &size);
                
                for(fourth = return_flag(third); fourth <= 62; fourth ++){
                    fourth_char = return_char(fourth, &size);
                    
                    word = malloc (size + 2);
                                    
                    int i = 0;
                                    
                    append_char_function(first_char, word, &i);
                    append_char_function(second_char, word, &i);
                    append_char_function(third_char, word, &i);
                    append_char_function(fourth_char, word, &i);;
                    i++;
                    word[i] = '\0';

                    // runTelNetBruteForce(hostname, word);
                }
            }
        }
    }
}
