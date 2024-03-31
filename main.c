#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

int output_file;
// sizeof bytes of the path, path, acc_rights, size

void save_file_state(const char* file_name, struct stat* file_stat) {
    if (!S_ISREG(file_stat->st_mode)) {
        return;
    }
    printf("Writing state of %s\n", file_name);
    size_t len_file_name = strlen(file_name);
    write(output_file, &len_file_name, sizeof(size_t));
    write(output_file, " ", 1);
    write(output_file, file_name, len_file_name);
    write(output_file, " ", 1);
    write(output_file, &(file_stat->st_mode), sizeof(file_stat->st_mode));

    write(output_file, "\n", 1);
}

void iterate_directories(const char* dir_name) {
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
        printf("%s\n", next_dir_name);
        struct stat buf;
        if (stat(next_dir_name, &buf) != 0) {
            exit(1); 
        }
        if(S_ISDIR(buf.st_mode) && strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
            iterate_directories(next_dir_name);
        }
        save_file_state(next_dir_name, &buf);
    }
    closedir(parent);
}

int main(int argc, char** argv) {
    output_file = open(".files_data", O_RDWR | O_CREAT, 0777);
    if (output_file == -1) {
        printf("[error] Could not open output file\n");
        exit(1);
    }
    iterate_directories("/os/project");
    close(output_file);
    return 0;
}
