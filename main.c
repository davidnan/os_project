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
int input_file;
// sizeof bytes of the path, path, acc_rights, size

void save_file_state(const char* file_name, struct stat* file_stat) {
    if (!S_ISREG(file_stat->st_mode)) {
        return;
    }
    int len_file_name = strlen(file_name);
    write(output_file, &len_file_name, sizeof(len_file_name));
    write(output_file, file_name, len_file_name);
    write(output_file, &(file_stat->st_mode), sizeof(file_stat->st_mode));
    write(output_file, &(file_stat->st_size), sizeof(file_stat->st_size));
    write(output_file, &(file_stat->st_ino), sizeof(file_stat->st_ino));
}

int read_file_state() {
    int number = 0;
    read(input_file, &number, sizeof(int));
    if(number >= 1024){
        printf("[error] Something wrong with the read number, might smash the stack\n");
        return 1;
    }
    if (number == 0) {
        return 1;
    }
    printf("number: %d\n", number);
    char buf[1024] = "\0";
    read(input_file, &buf, number);
    printf("dir: %s\n", buf);
    mode_t acc_right;
    read(input_file, &acc_right, 4);
    printf("acc_rights: %x\n", acc_right);
    off_t file_size;
    read(input_file, &file_size, sizeof(file_size));
    printf("size: %ldb\n", file_size);
    ino_t inode_id;
    read(input_file, &inode_id, sizeof(inode_id));
    printf("inode_id: %ld\n", inode_id);
    printf("%ld %d %ld %ld %ld\n", sizeof(int), number, sizeof(acc_right), sizeof(file_size), sizeof(inode_id));
    return 0;
}

void create_snapshot(const char* dir_name) {
    DIR* parent = opendir(dir_name);
    struct stat buf;
    if (stat(dir_name, &buf) != 0) {
        exit(1); 
    }
    save_file_state(dir_name, &buf);
    if (parent == NULL) {
        return;
    }
    struct dirent* file;
    while((file = readdir(parent)) != NULL) {
        char next_dir_name[1024] = "\0";
        strcat(next_dir_name, dir_name);
        strcat(next_dir_name, "/");
        strcat(next_dir_name, file->d_name);
        printf("%s\n", next_dir_name);
        if(S_ISDIR(buf.st_mode) && strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
            create_snapshot(next_dir_name);
        }
    }
    closedir(parent);
}

int main(int argc, char** argv) {
    output_file = open(".files_data", O_WRONLY | O_CREAT, 0777);
    if (output_file == -1) {
        printf("[error] Could not open output file\n");
        exit(1);
    }
    if (argc == 1) {
        printf("[error] Not enought arguments\n");
        close(output_file);
        exit(1);
    }
    int argument_count = argc - 1;
    while(argument_count--) {
        create_snapshot(argv[argument_count + 1]);
    } 
    close(output_file);

    input_file = open(".files_data", O_RDONLY, 0777);
    if (input_file == -1) {
        printf("[error] Could not open input file\n");
        close(input_file);
        exit(1);
    }
    
    while(read_file_state() == 0);
    close(input_file);
    return 0;
}
