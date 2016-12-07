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

// int main(int argc, char **argv){
//     timestamp();
//     char *result;
//     char *cadeia;
//     if(argv[1]!= NULL){
//         cadeia = malloc (sizeof(argv[1]) * sizeof(char *));
//         strcpy(cadeia,argv[1]);
//         RemoveSpaces(cadeia);
//         if(argv[2]!=NULL){
//     	    result = findWord(argv[2]);    
//         	if(result != NULL)
//                 printf("A Dica é:\n %s\n", makeXOR(result, argv[1]));
//             else
//                 printf("\nEPA, nao achei!\n");
//     	}else{
//     	    printf("Valor do MD5 chave não pode ser nulo\n");
//     	}
//     }else{
//         printf("Valor da  não pode ser nulo\n");
//     }
//     timestamp();
//     return 1;
// }