#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>

char *get_subdir_path(const char *src_path, const char *subdir_name);

void write_results(char *name, int cmp_status);

char *get_compile_error_msg(const char *name);

int main(int argc, char **argv){

    DIR     *dir, *subdir;
    struct  dirent *entry, *subentry;
    char    input_buffer[256], output_buffer[256], comp_output[256];
    char    src_dir_path[128], input_file_path[128], output_file_path[128];
    FILE    *cfg, *input, *output;
    char    *subdir_path;
    int     status, pipe1[2], pipe2[2];

    if (argc < 2){
        printf("no config file path provided\n");
        exit(1);
    }

    /* READING THE CONFIG FILE */
    cfg = fopen(argv[1], "r");
    assert(cfg);

    fgets(src_dir_path, 128, cfg);     // 1st line - src dir
    fgets(input_file_path, 128, cfg);  // 2nd line - input
    fgets(output_file_path, 128, cfg); // 3rd line - output
  
    src_dir_path[strlen(src_dir_path)-1] = '\0'; 
    input_file_path[strlen(input_file_path)-1] = '\0'; 
    output_file_path[strlen(output_file_path)-1] = '\0';
   
    /* OPEN+READ INPUT & OUTPUT FILES */
    input = fopen(input_file_path, "r");
    assert(input);
    output = fopen(output_file_path, "r");
    assert(output);
    fread(input_buffer, sizeof(input_buffer), 1, input);
    fread(output_buffer, sizeof(output_buffer), 1, output);

    /* SCAN THE SRC DIR */
    dir = opendir(src_dir_path);
    assert(dir);

    // scan all the sub dirs
    while ((entry = readdir(dir)) != NULL){

        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;

        if (entry->d_type == DT_DIR) { // we've got a subdir!
              
            subdir_path = get_subdir_path(src_dir_path, entry->d_name);
            subdir = opendir(subdir_path);
            assert(subdir);
           
            while ((subentry = readdir(subdir)) != NULL){
                 
                 pipe(pipe1); // TODO: errors
                 pipe(pipe2);

                 if (!strcmp(subentry->d_name, ".") || !strcmp(subentry->d_name, ".."))
                    continue;
                 
                 if (!fork()){ // compile
                    close(2); // keep the console clean from gcc errors and TODO: redirect stderr to somewhere else
                    char *args[] = {"gcc", strcat(subdir_path, subentry->d_name), "-o", "comp.o", NULL};
                    execv("/usr/bin/gcc", args);
                    perror("compile");
                    exit(1);
                 }
                 else {
                    wait(&status);
                    if (WEXITSTATUS(status) != 0) { // compilation error                    
                        char *error = get_compile_error_msg(entry->d_name);
                        perror(error);
                        write_results(entry->d_name, 1);
                        continue;
                    }
                 }
                 if (!fork()){ // run, if compile succeeded  
                    close(0);
                    dup(pipe1[0]);
                    close(1);
                    dup(pipe2[1]);
                    execl("./comp.o", "comp.o", NULL);
                    perror("run");
                    exit(1);
                 }
                 else { // get output and write it to a file
                    close(pipe1[0]); 
                    close(pipe2[1]);
                    write(pipe1[1], input_buffer, sizeof(input_buffer));
                    read(pipe2[0], comp_output, sizeof(comp_output));
                    write_results(entry->d_name, strncmp(output_buffer, comp_output, strlen(output_buffer)));
                    wait(&status);   
                 }
                 
                 if (!fork()){ // delete
                    int ret = remove("comp.o");
                    assert(ret >= 0);
                    exit(0);
                 }
                 else
                    wait(&status);
                 
                 close(pipe1[0]);
                 close(pipe1[1]);
                 close(pipe2[0]);
                 close(pipe2[1]);   
            }

            free(subdir_path);
            closedir(subdir);
        }
    }

    /* CLEANING  */
    fclose(cfg);
    fclose(input);
    fclose(output);
    closedir(dir);
    
    printf("DONE!\n");
    return 0;
     
}


char *get_subdir_path(const char *src_path, const char *subdir_name){

    char *subdir_path = (char*)malloc(strlen(src_path)+strlen(subdir_name)+1);
    strcpy(subdir_path, src_path);
    strcat(subdir_path, subdir_name);
    strcat(subdir_path, "/");

    return subdir_path;
}

void write_results(char *name, int cmp_status){

    char *result_line = (char*)malloc(strlen(name)+5);
    strcpy(result_line, name);
    strcat(result_line, ",");

    FILE *results = fopen("results.csv", "a+");
    assert(results);

    if (cmp_status == 0) {
        strcat(result_line, "100\n");
        fwrite(result_line, (size_t)strlen(result_line), 1, results); // TODO: errors
    }
    else {
        strcat(result_line, "0\n");
        fwrite(result_line, (size_t)strlen(result_line), 1, results); // TODO: errors
    }

    free(result_line);
}

char *get_compile_error_msg(const char *name){

    char *msg = "compilation error [";
    char *error = (char*)malloc(sizeof(msg)+sizeof(name)+1);
    strcpy(error, msg);
    strcat(error, name);
    strcat(error, "]");
    return error;
}
