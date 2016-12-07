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

#define REG_HEX "^[a-f]$|^[A-F]$"

//http://stackoverflow.com/a/8389763
//gcc brutexor.c -o brutextor -lcrypto -lssl
void timestamp()
{
    time_t ltime; /* calendar time */
    ltime=time(NULL); /* get current cal time */
    printf("%s",asctime( localtime(&ltime) ) );
}

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
char * findWord(char * MD5){
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
                    
                    for(fifth = return_flag(fourth); fifth <= 62; fifth ++){
                        fifth_char = return_char(fifth, &size);
                        
                        for(sixth = return_flag(fifth); sixth <= 62; sixth ++){
                            sixth_char = return_char(sixth , &size);
                            
                            for(seventh = return_flag(sixth); seventh <= 62; seventh ++){
                                seventh_char = return_char(seventh, &size);
                                
                                for(eighth = 0; eighth < 62; eighth ++){
                                    eighth_char = retriveChar(eighth);
                                    word = malloc (size + 2);
                                    
                                    int i = 0;
                                    
                                    append_char_function(first_char, word, &i);
                                    append_char_function(second_char, word, &i);
                                    append_char_function(third_char, word, &i);
                                    append_char_function(fourth_char, word, &i);
                                    append_char_function(fifth_char, word, &i);
                                    append_char_function(sixth_char, word, &i);
                                    append_char_function(seventh_char, word, &i);
                                    word[i] = eighth_char;
                                    i++;
                                    word[i] = '\0';
 									//str2md5(word,buffer, strlen(word));
                                    if(strcmp(buffer, MD5) == 0)
                                        return word;
                                    size = 0;
                                    free(word);
                                }
                            }
                        }
                    }
                }
            }
        }   
    }
	return NULL;
}

/**
*   Função que faz a validação da entrada strValidate de acordo com o regex definido no pattern.
**/
int regexValidation(char *strValidate, char * pattern){
    regex_t reg;

    /* compila a ER passada em argv[1]
     * em caso de erro, a função retorna diferente de zero */
    if (regcomp(&reg , pattern, REG_EXTENDED|REG_NOSUB) != 0) {
        fprintf(stderr,"erro regcomp\n");
        exit(1);
    }
    /* tenta casar a ER compilada (&reg) com a entrada (argv[2])
     * se a função regexec retornar 0 casou, caso contrário não */
    if ((regexec(&reg, strValidate, 0, (regmatch_t *)NULL, 0)) == 0)
        return 1;
    else
        return 0;
}

// Retorna o inteiro correspondente ao hexa definido no caracter c
int hex_to_int(char c){
	char str[2];
	str[0] = c;
	str[1] = '\0';
	if(regexValidation(str,REG_HEX)){
		if(c == 'a' || c == 'A')
			return 10;
		else if(c == 'b' || c == 'B')
			return 11;
		else if(c == 'c' || c == 'C')
			return 12;
		else if(c == 'd' || c == 'D')
			return 13;
		else if(c == 'e' || c == 'E')
			return 14;
		else if(c == 'f' || c == 'F')
			return 15;
		else
			return 0;
	}
	else
		 return (int) strtol(str, NULL, 10);
}

// Retorna o inteiro correspondende para um hexa com dois digitos.
//http://stackoverflow.com/a/5403170
int hex_to_ascii(char c, char d){
        int high = hex_to_int(c) * 16;
        int low = hex_to_int(d);
        return high+low;
}

// Retorna o ASCII de uma string em Hexa
void returnASCIIString(char *string, char *cadeia){	
	int index = 0;
	int i;
	char buf = 0;
	for(i = 0; i < 41; i++){
        if(i % 2 != 0){
                string[index] = hex_to_ascii(buf, cadeia[i]);
				index++;
        }else{
                buf = cadeia[i];
        }
	}		
}

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