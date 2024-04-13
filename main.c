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

typedef struct file_state{
    char name[1024];
    unsigned int name_length;
    time_t mod_time;
    mode_t acc_rights;
    off_t size;
    ino_t ino_id;
} file_state_t;

void print_file_data(file_state_t* file_data) {
    printf("file name length: %d\n", file_data->name_length);
    printf("file name: %s\n", file_data->name);
    printf("last time modified: %ld\n", file_data->mod_time);
    printf("acc_rights: %x\n", file_data->acc_rights);
    printf("size: %ldb\n", file_data->size);
    printf("inode_id: %ld\n", file_data->ino_id);
}

void create_state_from_stat(const char* file_name, struct stat* file_stat, file_state_t* state) {
    state->name_length = strlen(file_name);
    strcpy(state->name, file_name);
    state->mod_time = file_stat->st_mtime;
    state->acc_rights = file_stat->st_mode;
    state->size = file_stat->st_size;
    state->ino_id = file_stat->st_ino;
}

void save_file_state(file_state_t* state) {
    if (!S_ISREG(state->acc_rights)) {
        return;
    }
    write(output_file, &state->name_length, sizeof(state->name_length));
    write(output_file, &state->name, state->name_length);
    write(output_file, &(state->mod_time), sizeof(state->mod_time));
    write(output_file, &(state->acc_rights), sizeof(state->acc_rights));
    write(output_file, &(state->size), sizeof(state->size));
    write(output_file, &(state->ino_id), sizeof(state->ino_id));
}

int read_file_state(file_state_t* file_data) {
    file_data->name_length = 0;
    read(input_file, &file_data->name_length, sizeof(file_data->name_length));
    if(file_data->name_length >= 1024){
        printf("[error] Something wrong with the read number, might smash the stack\n");
        return 1;
    }
    if (file_data->name_length == 0) {
        return 1;
    }
    char buf[1024] = "\0";
    read(input_file, &buf, file_data->name_length);
    strcpy(file_data->name, buf);
    read(input_file, &file_data->mod_time, sizeof(file_data->mod_time));
    read(input_file, &file_data->acc_rights, sizeof(file_data->acc_rights));
    read(input_file, &file_data->size, sizeof(file_data->size));
    read(input_file, &file_data->ino_id, sizeof(file_data->ino_id));
    // print_file_data(file_data);
    return 0;
}

void refresh_snapshot(int files_len, char* files[]) {
    // read all data in last snapshot, whithout adding the files that are added in this snapshot
    file_state_t all_tracked_file_states[1000];
    int tracked_file_count = 0;
    ino_t inode_blacklist[1000]; int blacklisted_cnt = 0;
    file_state_t state;
    for (int i = 0; i < files_len; i++) {
        struct stat current_status;
        if (stat(files[i], &current_status) != 0) {
            continue;
        }
        inode_blacklist[blacklisted_cnt++] = current_status.st_ino;
    }
    

    input_file = open(".files_data", O_RDONLY, 0777);
    if (input_file != -1) {
        while(read_file_state(&state) == 0){
            int is_bl = 0;
            for(int i = 0; i < blacklisted_cnt; i++) {
                if (state.ino_id == inode_blacklist[i]) {
                    is_bl = 1;
                    break;
                }
            }
            if(is_bl == 0) {
                all_tracked_file_states[tracked_file_count++] = state;
            }
        }
        close(input_file);
        input_file = 0;
    }

    // put the data in the file. The id of the file will remain in memory to add the new tracked files
    output_file = open(".files_data", O_WRONLY | O_TRUNC | O_CREAT, 0777);
    if (output_file == -1) {
        printf("[error] Could not open output file\n");
        exit(1);
    }
    for (int i = 0; i < tracked_file_count; i++) {
        save_file_state(&all_tracked_file_states[i]);
    }
}

void create_snapshot(char* dir_name) {
    printf("%s\n", dir_name);
    if (dir_name == NULL || dir_name[0] == '\0') {
        return;
    }
    DIR* parent = opendir(dir_name);
    struct stat buf;
    if (stat(dir_name, &buf) != 0) {
        exit(1); 
    }
    file_state_t state;
    create_state_from_stat(dir_name, &buf, &state);
    save_file_state(&state);
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


void add_files_to_tracking(int argument_count, char* files[]) {
    refresh_snapshot(argument_count, files);

    for (int i = 0; i < argument_count; i++) {
        if(fork()) {
            sleep(1);
        }
        else {
            printf("child process %s\n", files[i]);
            create_snapshot(files[i]);
            exit(1);
        }
    }

    // while(argument_count--) {
    //     create_snapshot(files[argument_count]);
    // } 
    close(output_file);
    output_file = 0;
}

void print_data_from_tracking() {
    input_file = open(".files_data", O_RDONLY, 0777);
    if (input_file == -1) {
        printf("[error] Could not open input file\n");
        close(input_file);
        exit(1);
    }
    file_state_t file_data;
    while(read_file_state(&file_data) == 0) {
        print_file_data(&file_data);
    }
    close(input_file);
    input_file = 0;
}

int print_file_status(file_state_t* file_data) {
    struct stat current_status;
    if (stat(file_data->name, &current_status) != 0) {
        return 0;
    }
    if (file_data->mod_time != current_status.st_mtime) {
        printf("modified: %s\n", file_data->name);
    }
    if (file_data->acc_rights != current_status.st_mode) {
        printf("modified mode: %s   %x -> %x\n", file_data->name, file_data->acc_rights, current_status.st_mode);
    }


    return 0;
}

void print_status() {
    input_file = open(".files_data", O_RDONLY, 0777);
    if (input_file == -1) {
        printf("[error] Could not open input file\n");
        close(input_file);
        exit(1);
    }
    file_state_t file_data;
    while(read_file_state(&file_data) == 0){
        print_file_status(&file_data);
    }
    close(input_file);
    input_file = 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("[error] Not enought arguments\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "add") == 0) {
        add_files_to_tracking(argc - 2, argv + 2);
    }
    else if (strcmp(argv[1], "data") == 0) {
        print_data_from_tracking();
    }
    else if (strcmp(argv[1], "status") == 0) {
        print_status();
    }
    else {
        printf("Unknown command\nAvailable commands: add [file1 file2 ...], data, status\n");
    }


    return 0;
}
