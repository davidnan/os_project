#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>


void print_directories(const char* dir_name) {
    DIR* parent = opendir(dir_name);
    if (parent == NULL) {
        return;
    }
    struct dirent* file;
    while((file = readdir(parent)) != NULL) {
        char next_dir_name[1000] = "";
        strcat(next_dir_name, dir_name);
        strcat(next_dir_name, "/");
        strcat(next_dir_name, file->d_name);
        strcat(next_dir_name, "\0");
        printf("%s\n", next_dir_name);
        struct stat* buf;
        if (stat(next_dir_name, buf) != 0) {
            printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
            exit(2); 
        }
        if(S_ISDIR(buf->st_mode) && strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
            print_directories(next_dir_name);
        }
    }
    closedir(parent);
}

int main(int argc, char** argv) {
    print_directories("/os");

    return 0;
}
