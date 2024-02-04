//
// Created by Diego Damian on 11/9/23.
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define BUFFER_MAX 1024	//use to define the maximum buffer space for reading
//Note: This is only a setting, set this to 1 will also work normally

struct Node{
    char *wd;
    int count;
    struct Node *next;
};
struct Node* memory;	//Stores names and counts of each word
char *bbuffer;	//Stores extra chars not ended at the end of the buffer space
int bbufferstat = 0;	//Stores storing status of the bbuffer char array
int bbuffersize = BUFFER_MAX;	//Stores current size of bbuffer, use for extreme cases

//ExamineWord: A subfunction for WordCount, used to further distinguish the word, combine bbuffer spaces, remove irregular hyphens and stores word count each by each
//Using ASCII values to detect correspond characters, see ASCII table for each

void ExamineWord(char buffer[], int start, int end){
    if(bbufferstat != 0 && start == 0){	//Major difference: If word seperated
        for(int i=0;i<end+1;i++){
            bbuffer[bbufferstat+i] = buffer[i];
        }
        end += bbufferstat;
        bbufferstat = 0;
        ExamineWord(bbuffer, start, end);
        for(int j=0;j<bbuffersize;j++){
            bbuffer[j] = 0;
        }	//if there's a bbuffer waited to plug current subword, execute pluggings and reset bbufferstat to avoid double-plugs, then call recursive for plugged-word
    }
    else{	//Major difference: If no word seperated
        int target = -1;
        for(int i=start;i<end+1;i++){
            if(buffer[i] == 45){
                target = i;
                break;
            }	//Check if bbuffer combined word has irregular hyphens
        }
        if(target != -1){
            if(target == start){
                if(start < end)	ExamineWord(buffer, start+1, end);
            }	//avoid single hyphens being counted as words
            else if(target == end){
                if(start < end) ExamineWord(buffer, start, end-1);
            }	//same as above
            else if(buffer[target+1] == 45){
                ExamineWord(buffer, start, target-1);
                if(target != end-1){
                    ExamineWord(buffer, target+2, end);
                }
                //avoid calling ExamineWord for no chars after double hyphen
            }
            else{
                target = -1;
            }
        }
        if(target == -1){
            char realword[end-start+2];
            strncpy(realword, &buffer[start], end-start+1);
            realword[end-start+1] = '\0';
            bool alreadystored = false;
            struct Node *temp = memory;
            while(temp->next != NULL){
                if(alreadystored) break;
                temp = temp->next;
                if(strcmp(realword, temp->wd) == 0){
                    temp->count ++;
                    alreadystored = true;
                }
            }
            if(!alreadystored){
                struct Node *newword = malloc(sizeof(struct Node));
                newword->wd = malloc(strlen(realword)+1);
                strcpy(newword->wd, realword);
                newword->count = 1;
                newword->next = NULL;
                temp->next = newword;
            }
        }
    }
}

//WordCount: The core function used to calculate the word count of a single file
//Will first do several processes like removing most punctuations and overflow bytes, then cut the readings into pieces by examine spaces, and use subfuncs to go through them
//Using ASCII values to detect correspond characters, see ASCII table for each

void WordCount(int fp){
    char buffer[BUFFER_MAX];
    int contains = 1;	//A variable used to determine if reading has error
    int start = 0, end = BUFFER_MAX-1;	//Variables to determine start
    while((contains = read(fp, buffer, sizeof(buffer))) > 0){
        if(contains == -1){
            close(fp);
            printf("UnexpectedError:Error Readings from File %d\n", fp);
            exit(1);
        }
        if(contains < BUFFER_MAX){	//replace overflow bytes with empty spaces
            for(int ct = contains;ct<BUFFER_MAX;ct++) buffer[ct] = 32;
        }
        for(int i=0;i<BUFFER_MAX;i++){	//replace normal punctuations with empty spaces
            if((buffer[i]>=33 && buffer[i]<=38)||(buffer[i]>=40 && buffer[i]<=44)||(buffer[i]>=46 && buffer[i]<=64)||(buffer[i]>=91 && buffer[i]<=96)||(buffer[i]>=123)){
                buffer[i] = 32;
            }
        }
        for(int i=0;i<BUFFER_MAX;i++){
            if(buffer[i] <= 32 || buffer[i]>=127){
                end = i-1;	//Find empty space, seperate words
                if(start <= end){	//Rule out chances of continuous spaces
                    ExamineWord(buffer, start, end);
                }
                else if(end == -1 && bbufferstat != 0){	//If bbuffer stores stop before space
                    char *tempbuffer = malloc(bbuffersize*sizeof(char));
                    strcpy(tempbuffer, bbuffer);
                    int tempbufferlength = bbufferstat-1;
                    bbufferstat = 0;
                    for(int b=0;b<bbuffersize;b++){
                        bbuffer[b] = 0;
                    }
                    ExamineWord(tempbuffer, 0, tempbufferlength);
                    free(tempbuffer);
                    //chars stored in bbuffer able to form a complete word, allocate another char to avoid double use on combination detecting for bbuffer and buffer
                }
                start = i+1;
            }
        }
        if((end != BUFFER_MAX-2) && (buffer[BUFFER_MAX-1] != 32)){
            if(bbufferstat == 0){
                for(int j=start;j<BUFFER_MAX;j++){
                    bbuffer[j-start] = buffer[j];
                }
                bbufferstat = BUFFER_MAX - start;
            }//If buffer space does not stop at an empty space, stores the first part of this word and then wait still next read, combined with second part of the word
            else{
                for(int j=start;j<BUFFER_MAX;j++){
                    bbuffer[j-start+bbufferstat] = buffer[j];
                }
                bbufferstat = bbufferstat + BUFFER_MAX - start;
            }//If bbuffer space is not empty and we get other parts needed to be pieced up, just directly store into bbuffer
        }
        start = 0;
        end = BUFFER_MAX - 1;
    }
}

//BeenThrough: a recursive function used to go through all subdirectories and files in the given file path, and calls the word count check function
//Note that in original struct dirent definition, d_type DT_DIR = 4, DT_REG = 8

void BeenThrough(char *path){
    DIR *dirpath = opendir(path);
    if(dirpath == NULL){
        printf("UnexpectedError:UnsuccessfulOpeningDirectory: %s\n", path);
        return;
    }
    struct dirent *entry;
    while((entry = readdir(dirpath)) != NULL){
        if((strcmp(entry->d_name, ".") == 0)||(strcmp(entry->d_name, "..") == 0)){
            continue;
            //skip entries if they belonged to parent directories or themselves
        }
        char newpath[strlen(path)+strlen(entry->d_name)+2];
        snprintf(newpath, sizeof(newpath), "%s/%s", path, entry->d_name);
        //copies the original path and creates a new path for current entry
        if(entry->d_type == 4){
            BeenThrough(newpath);
            //if d_type == DT_DIR, it's a directory, been through it again
        }
        else if(entry->d_type == 8){
            //if d_type == DT_REG, it's a regular file
            char *search = strstr(newpath, ".txt");
            if((search != NULL) && ((search+strlen(".txt")) == (newpath+strlen(newpath)))){
                //if this file is determined to be a txt, read it
                //the second part is for making sure that ".txt" is found at the end of the file name, and the given file is really a txt file, to avoid files like 1.txt.c
                int fp = open(newpath,O_RDWR);
                if(fp == -1) printf("InvalidFileReadError: %s\n",newpath);
                else{
                    struct stat file_info;
                    stat(newpath, &file_info);
                    bbuffer = realloc(bbuffer, file_info.st_size);
                    bbuffersize = file_info.st_size;
                    WordCount(fp);
                    close(fp);
                }
            }
        }
    }
    if(closedir(dirpath) == -1){
        printf("UnexpectedError:UnsuccessfulClosingDirectory: %s\n", path);
        exit(1);
    }
}

//DesendOutput: subfunction for main to choose and output the word in the list that has maximum appearance count, lexicographically, and then free it

void DesendOutput(struct Node *memory){
    struct Node *prev = memory;
    struct Node *target = memory->next;
    memory = memory->next;
    int max = memory->count;
    char* word = memory->wd;
    while(memory->next != NULL){
        if(max < memory->next->count){
            max = memory->next->count;
            word = memory->next->wd;
            prev = memory;
            target = memory->next;
        }
        else if(max == memory->next->count){
            if(strcmp(word, memory->next->wd)>0){
                max = memory->next->count;
                word = memory->next->wd;
                prev = memory;
                target = memory->next;
            }
        }
        memory = memory->next;
    }
    write(STDOUT_FILENO, word, strlen(word));
    write(STDOUT_FILENO, " ", 1);
    char *buf = malloc(snprintf(NULL, 0, "%d", max)+1);
    int cnum = sprintf(buf, "%d", max);
    write(STDOUT_FILENO, buf, cnum);
    write(STDOUT_FILENO, "\n", 1);
    prev->next = prev->next->next;
    free(target->wd);
    free(target);
    free(buf);
}

//main function, reads in one or several arguments, and use it or them to detect and read the file, then provides the word counts for all of them

int main(int argc, char* argv[]){
    int i=0;			//Uses for determine number of arguments for further
    while(argv[i+1] != 0) i++;	//Note that arguments with names start from argv[1]
    if(i == 0){
        printf("UnexpectedError:NoValidArguments\n");
        exit(1);
    }
    memory = malloc(sizeof(struct Node));	//Basic Node, don't contain anything about word
    bbuffer = malloc(sizeof(char));	//Basic for not calling realloc on non-malloc variable
    memory->next = NULL;
    for(int j=1;j<i+1;j++){
        DIR *path = opendir(argv[j]);
        if(!path){
            int fp = open(argv[j],O_RDWR);
            if(fp == -1){
                printf("InvalidArgumentError:Argument%d, %s\n",j, argv[j]);
                //Note: If the given argument is a file name, but the program doesn't have read access to it, will also return this error
            }
            else{
                struct stat file_info;
                stat(argv[j], &file_info);
                bbuffer = realloc(bbuffer, file_info.st_size);
                bbuffersize = file_info.st_size;
                WordCount(fp);
                close(fp);
            }
        }
        else{
            closedir(path);
            BeenThrough(argv[j]);
        }
    }
    while(memory->next != NULL){
        DesendOutput(memory);	//Do write outputs and free Nodes
    }
    free(memory);	//Note that base Node memory is not freed and needs extra free
    free(bbuffer);
}