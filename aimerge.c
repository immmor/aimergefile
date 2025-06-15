#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#define MAX_LINE_LENGTH 1024
#define DEFAULT_OUTPUT_FILE "aimerged.txt"

void print_tree(const char* path, int depth, const char* prefix, int is_last, FILE* output_file) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    char full_path[MAX_LINE_LENGTH];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        snprintf(full_path, MAX_LINE_LENGTH, "%s/%s", path, entry->d_name);
        count++;
    }
    rewinddir(dir);

    int current = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        current++;
        snprintf(full_path, MAX_LINE_LENGTH, "%s/%s", path, entry->d_name);
        int is_last_entry = (current == count);

        if (entry->d_type == DT_DIR) {
            fprintf(output_file, "%s%s%s/\n", prefix, is_last_entry ? "└── " : "├── ", entry->d_name);
            char new_prefix[100];
            snprintf(new_prefix, sizeof(new_prefix), "%s%s    ", prefix, is_last_entry ? "    " : "|   ");
            print_tree(full_path, depth + 1, new_prefix, is_last_entry, output_file);
        } else {
            fprintf(output_file, "%s%s%s\n", prefix, is_last_entry ? "└── " : "├── ", entry->d_name);
        }
    }

    closedir(dir);
}

void append_file_to_output(const char *input_filename, const char *output_filename) {
    FILE *input_file = fopen(input_filename, "r");
    if (!input_file) {
        perror("Error opening input file");
        return;
    }

    FILE *output_file = fopen(output_filename, "a");
    if (!output_file) {
        perror("Error opening output file");
        fclose(input_file);
        return;
    }

    // 打印分隔线
    fprintf(output_file, "----------------------------------------\n");

    // 将文件绝对路径写入输出文件
    char abs_path[PATH_MAX];
    if (realpath(input_filename, abs_path) != NULL) {
        fprintf(output_file, "%s\n", abs_path);
    } else {
        fprintf(output_file, "%s\n", input_filename);
    }

    // 打印分隔线
    fprintf(output_file, "----------------------------------------\n");

    // 读取输入文件内容并写入输出文件
    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, MAX_LINE_LENGTH, input_file)) {
        fprintf(output_file, "%s", buffer);
    }

    // 添加一个空行分隔
    fprintf(output_file, "\n");

    fclose(input_file);
    fclose(output_file);
}

int main(int argc, char *argv[]) {
    const char *output_filename = DEFAULT_OUTPUT_FILE; // 默认输出文件名
    int opt;


    // 处理命令行参数
    while ((opt = getopt(argc, argv, "d:b")) != -1) {
        switch (opt) {
            case 'd':
                output_filename = optarg; // 指定输出文件名
                break;

            default: /* '?' */
                fprintf(stderr, "Usage: %s [-d output_file] <input_file1> <input_file2> ...\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    // 跳过已处理的参数
    argc -= optind;
    argv += optind;

    // 检查是否至少提供了一个输入文件
    if (argc < 1) {
        fprintf(stderr, "Usage: %s [-d output_file] <input_file1> <input_file2> ...\n", argv[0]);
        return 1;
    }

    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Error clearing output file");
        return 1;
    }
    
    fprintf(output_file, "当前目录: %s\n", getcwd(NULL, 0));
    print_tree(getcwd(NULL, 0), 0, "", 0, output_file);
    fprintf(output_file, "\n");
    
    fclose(output_file);

    for (int i = 0; i < argc; i++) {
        append_file_to_output(argv[i], output_filename);
    }

    printf("Files merged successfully into %s\n", output_filename);
    return 0;
}
