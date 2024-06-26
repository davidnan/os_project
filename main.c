#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>

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
    printf("\nfile name length: %d\n", file_data->name_length);
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

void write_to_file(int fd, file_state_t* state) {
    write(fd, &state->name_length, sizeof(state->name_length));
    write(fd, &state->name, state->name_length);
    write(fd, &(state->mod_time), sizeof(state->mod_time));
    write(fd, &(state->acc_rights), sizeof(state->acc_rights));
    write(fd, &(state->size), sizeof(state->size));
    write(fd, &(state->ino_id), sizeof(state->ino_id));
}

void save_file_state(file_state_t* state) {
    write_to_file(output_file, state);
}

int read_file(int fd, file_state_t* file_data) {
    file_data->name_length = 0;
    read(fd, &file_data->name_length, sizeof(file_data->name_length));
    if(file_data->name_length >= 1024){
        printf("[error] Something wrong with the read number, might smash the stack\n");
        return 1;
    }
    if (file_data->name_length == 0) {
        return 1;
    }
    char buf[1024] = "\0";
    read(fd, &buf, file_data->name_length);
    strcpy(file_data->name, buf);
    read(fd, &file_data->mod_time, sizeof(file_data->mod_time));
    read(fd, &file_data->acc_rights, sizeof(file_data->acc_rights));
    read(fd, &file_data->size, sizeof(file_data->size));
    read(fd, &file_data->ino_id, sizeof(file_data->ino_id));
    // print_file_data(file_data);
    return 0;

}

int read_input_file_state(file_state_t* file_data) {
    return read_file(input_file, file_data);
}

void add_to_blacklist(ino_t* inode_blacklist, int* blacklisted_cnt, char* file_name) {
    struct stat current_status;
    if (stat(file_name, &current_status) != 0) {
        exit(1);
    }

    if (S_ISREG(current_status.st_mode)) {
        // code in case of file
        inode_blacklist[(*blacklisted_cnt)++] = current_status.st_ino;
        return;
    }
    // code in case of directory
    DIR* parent = opendir(file_name);
    if (parent == NULL) {
        return;
    }
    struct dirent* file;
    while((file = readdir(parent)) != NULL) {
        char next_dir_name[1024] = "\0";
        strcat(next_dir_name, file_name);
        strcat(next_dir_name, "/");
        strcat(next_dir_name, file->d_name);
        if(S_ISDIR(current_status.st_mode) && strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
            add_to_blacklist(inode_blacklist, blacklisted_cnt, next_dir_name);
        }
    }
    closedir(parent);

}

void refresh_snapshot(int files_len, char* files[]) {
    // read all data in last snapshot, without adding the files that are added in this snapshot
    file_state_t all_tracked_file_states[1000];
    int tracked_file_count = 0;
    ino_t inode_blacklist[1000]; int blacklisted_cnt = 0;
    file_state_t state;
    for (int i = 0; i < files_len; i++) {
        add_to_blacklist(inode_blacklist, &blacklisted_cnt, files[i]);
    }

    input_file = open(".files_data", O_RDONLY, 0777);
    if (input_file != -1) {
        while(read_input_file_state(&state) == 0){
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

int verify_mal(char* file_name) {
    int pfd[2];
    int newfd;
    if (pipe(pfd) < 0) {
        perror("Pipe creation failed\n");
		exit(1);
    }

    int pid;
    if ((pid = fork()) < 0) {
        perror("fork failed\n");
        exit(1);
    }

    // parent code
    if (pid != 0) {
        close(pfd[1]);
        char rez;
        read(pfd[0], &rez, sizeof(char));
        return rez - '0';
    }

    // child code
    if (pid == 0) {
        if((newfd = dup2(pfd[1], 1)) < 0) {
            perror("Error when calling dup2\n");
            exit(1);
        }
        close(pfd[0]);  
        close(pfd[1]);  
        if(execlp("./verify_mal.sh", "./verify_mal.sh", file_name, NULL) < 0){
            char c = '1';
            write(newfd, &c, sizeof(char));
        }
        exit(0);
    }
    wait(NULL);
}

// If file_name represents a directory it checks if it is malicious, then adds it to tracking
// If file_name represents a directory it iterates throught the subfiles
void create_snapshot(char* file_name, int* pipe_lock, int* pipe_unlock, int* no_of_mal_files) {
    if (file_name == NULL || file_name[0] == '\0') {
        return;
    }
    struct stat buf;
    if (stat(file_name, &buf) != 0) {
        exit(1); 
    }
    file_state_t state;
    create_state_from_stat(file_name, &buf, &state);
    if (S_ISREG(state.acc_rights)) {
        // code in case of file
        if(verify_mal(file_name)) {
            (*no_of_mal_files)++;
            int pid = fork();
            if (pid < 0) {
                perror("Pipe creation failed\n");
                exit(1);
            }
            // child code
            if (pid == 0) {
                execlp("./mv_q.sh", "./mv_q.sh", file_name, NULL);
                exit(1);
            }
            wait(NULL);
            return;
        }
        int r;
        read(pipe_lock[0], &r, sizeof(int));
        write_to_file(output_file, &state);
        write(pipe_unlock[1], &r, sizeof(int));
        return;
    }
    // code in case of directory
    DIR* parent = opendir(file_name);
    if (parent == NULL) {
        return;
    }
    struct dirent* file;
    while((file = readdir(parent)) != NULL) {
        char next_dir_name[1024] = "\0";
        strcat(next_dir_name, file_name);
        strcat(next_dir_name, "/");
        strcat(next_dir_name, file->d_name);
        printf("%s\n", next_dir_name);
        if(S_ISDIR(buf.st_mode) && strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
            create_snapshot(next_dir_name, pipe_lock, pipe_unlock, no_of_mal_files);
        }
    }
    closedir(parent);
}


void add_files_to_tracking(int argument_count, char* files[]) {
    refresh_snapshot(argument_count, files);
    int pipe_lock[2];
    int pipe_unlock[2];
    int communication_pipe[2];
    if (pipe(pipe_lock) < 0) {
        perror("Pipe creation failed\n");
        exit(1);
    }
    if (pipe(pipe_unlock) < 0) {
        perror("Pipe creation failed\n");
        exit(1);
    }
    if (pipe(communication_pipe) < 0) {
        perror("Pipe creation failed\n");
        exit(1);
    }

    for (int i = 0; i < argument_count; i++) {
        int pid = fork();
        if (pid < 0) {
            perror("Fork failed");
            exit(1);
        }
        if(!pid) {
            // setting up the synchronization pipes inside the child process
            int r = 1;
            close(pipe_lock[1]);
            close(pipe_unlock[0]);

            close(communication_pipe[0]);

            int pid = getpid();
            int no_of_mal_files = 0;
            create_snapshot(files[i], pipe_lock, pipe_unlock, &no_of_mal_files);

            r = 2;
            // locking
            read(pipe_lock[0], &r, sizeof(int));
            // writing
            write(communication_pipe[1], &pid, sizeof(pid));
            write(communication_pipe[1], &no_of_mal_files, sizeof(no_of_mal_files));
            // unlocking
            write(pipe_unlock[1], &r, sizeof(int));

            close(pipe_lock[0]);
            close(pipe_unlock[1]);
            close(communication_pipe[1]);
            exit(1);
        }
    }
    close(pipe_lock[0]);
    close(pipe_unlock[1]);
    close(communication_pipe[1]);
    int r=1;
    write(pipe_lock[1], &r, sizeof(int));
    while(read(pipe_unlock[0], &r, sizeof(int))) {
        if (r == 2) {
        }
        write(pipe_lock[1], &r, sizeof(int));
    }

    int child_pid;
    int no_of_mal_files;
    while(read(communication_pipe[0], &child_pid, sizeof(child_pid))) {
        read(communication_pipe[0], &no_of_mal_files, sizeof(no_of_mal_files));
        printf("Child process %d having pid: %d had %d suspect files\n", child_pid - getpid(), child_pid, no_of_mal_files);
    }
    close(pipe_lock[1]);
    close(pipe_lock[0]);
    close(communication_pipe[0]);

    close(output_file);
    output_file = 0;
    int st = 0;
    int wpid;
    while ((wpid = wait(&st)) > 0);
}

void print_data_from_tracking() {
    input_file = open(".files_data", O_RDONLY, 0777);
    if (input_file == -1) {
        perror("Could not open input file\n");
        close(input_file);
        exit(1);
    }
    file_state_t file_data;
    while(read_input_file_state(&file_data) == 0) {
        print_file_data(&file_data);
    }
    close(input_file);
    input_file = 0;
}

int print_file_status(file_state_t* file_data) {
    DIR* parent = opendir(".");
    if (parent == NULL) {
        return 1;
    }
    int found = 0;
    struct dirent* file;
    while((file = readdir(parent)) != NULL) {
        if(file->d_ino == file_data->ino_id && strcmp(file->d_name, file_data->name) != 0) {
            found = 1;
            printf("renamed: %s -> %s\n", file_data->name, file->d_name);
            break;
        }
    }
    closedir(parent);

    // checking for a file that exists and doesnt have the name changed 
    struct stat current_status;
    if (stat(file_data->name, &current_status) != 0) {
        if (found == 0) {
            printf("deleted: %s\n", file_data->name);
        }
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
        perror("Could not open input file\n");
        close(input_file);
        exit(1);
    }
    file_state_t file_data;
    while(read_input_file_state(&file_data) == 0){
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
