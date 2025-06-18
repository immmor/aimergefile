#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <fnmatch.h>
#include <glob.h>

#ifdef _WIN32
#include <direct.h>
#define PATH_SEPARATOR '\\'
#define realpath(N,R) _fullpath((R),(N),_MAX_PATH)
#else
#define PATH_SEPARATOR '/'
#endif

#define MAX_LINE_LENGTH 1024
#define DEFAULT_OUTPUT_FILE "aimerged.txt"
#define AIMIGNORE_FILE ".aimignore"

// 定义一个结构体来存储忽略的路径
typedef struct IgnorePath {
    char* pattern;
    int is_dir;
    struct IgnorePath* next;
} IgnorePath;

// 全局变量：忽略路径链表的头指针
IgnorePath* ignore_head = NULL;
char* base_dir = NULL; // 存储基础目录路径

// 函数：添加忽略路径到链表
void add_ignore_path(const char* pattern, int is_dir) {
    IgnorePath* new_node = (IgnorePath*)malloc(sizeof(IgnorePath));
    if (!new_node) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    new_node->pattern = strdup(pattern);
    new_node->is_dir = is_dir;
    new_node->next = ignore_head;
    ignore_head = new_node;
}

// 获取相对路径
char* get_relative_path(const char* full_path, const char* base_path) {
    size_t base_len = strlen(base_path);
    if (strncmp(full_path, base_path, base_len) == 0) {
        const char* relative = full_path + base_len;
        // 跳过路径分隔符
        if (relative[0] == PATH_SEPARATOR) {
            relative++;
        }
        return strdup(relative);
    }
    return strdup(full_path);
}

int is_ignored(const char* full_path, int is_dir) {
    if (!base_dir) return 0;
    
    char* relative_path = get_relative_path(full_path, base_dir);
    if (!relative_path) return 0;
    
    IgnorePath* current = ignore_head;
    while (current) {
        // 对于目录模式，检查是否匹配目录本身或其子路径
        if (current->is_dir) {
            // 直接匹配目录名
            if (fnmatch(current->pattern, relative_path, FNM_PATHNAME | FNM_LEADING_DIR) == 0) {
                free(relative_path);
                return 1;
            }
            // 检查是否在被忽略的目录下
            size_t pattern_len = strlen(current->pattern);
            if (strncmp(relative_path, current->pattern, pattern_len) == 0 &&
                (relative_path[pattern_len] == '\0' || relative_path[pattern_len] == PATH_SEPARATOR)) {
                free(relative_path);
                return 1;
            }
        } else {
            // 对于文件模式，只有在类型匹配时才检查
            if (!is_dir && fnmatch(current->pattern, relative_path, FNM_PATHNAME) == 0) {
                free(relative_path);
                return 1;
            }
        }
        current = current->next;
    }
    
    free(relative_path);
    return 0;
}

// 函数：释放忽略路径链表的内存
void free_ignore_list() {
    IgnorePath* current = ignore_head;
    while (current) {
        IgnorePath* next = current->next;
        free(current->pattern);
        free(current);
        current = next;
    }
    if (base_dir) {
        free(base_dir);
    }
}

void parse_aimignore(const char* cwd) {
    FILE* ignore_file = fopen(AIMIGNORE_FILE, "r");
    if (!ignore_file) {
        return; // 如果文件不存在，直接返回
    }

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), ignore_file)) {
        // 去掉换行符
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0' || line[0] == '#') {
            continue; // 跳过空行和注释
        }

        // 处理路径，去掉开头的 ./ 
        char* pattern = line;
        if (strncmp(pattern, "./", 2) == 0) {
            pattern += 2; // 跳过 "./"
        }

        // 检查路径是否以分隔符结尾，表示文件夹
        int is_dir = 0;
        size_t len = strlen(pattern);
        if (len > 0 && pattern[len - 1] == PATH_SEPARATOR) {
            pattern[len - 1] = '\0'; // 去掉尾部的分隔符
            is_dir = 1;
        }

        // 跳过空的模式
        if (strlen(pattern) == 0) {
            continue;
        }

        // 添加到忽略列表（使用相对路径模式）
        add_ignore_path(pattern, is_dir);
    }
    fclose(ignore_file);
}

void print_tree(const char* path, int depth, const char* prefix, int is_last, FILE* output_file) {
    DIR* dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return;
    }

    struct dirent* entry;
    char full_path[MAX_LINE_LENGTH];
    int count = 0;

    // 首先计算有效条目数量
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, MAX_LINE_LENGTH, "%s%c%s", path, PATH_SEPARATOR, entry->d_name);
        
        struct stat statbuf;
        int is_dir = (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
        
        // 检查是否被忽略
        if (is_ignored(full_path, is_dir)) {
            continue;
        }
        
        count++;
    }
    rewinddir(dir);

    int current = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        snprintf(full_path, MAX_LINE_LENGTH, "%s%c%s", path, PATH_SEPARATOR, entry->d_name);
        
        struct stat statbuf;
        int is_dir = (stat(full_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode));
        
        // 检查是否被忽略
        if (is_ignored(full_path, is_dir)) {
            continue;
        }
        
        current++;
        int is_last_entry = (current == count);

        if (is_dir) {
            fprintf(output_file, "%s%s%s/\n", prefix, is_last_entry ? "└── " : "├── ", entry->d_name);
            char new_prefix[100];
            snprintf(new_prefix, sizeof(new_prefix), "%s%s", prefix, is_last_entry ? "    " : "|   ");
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

    fprintf(output_file, "----------------------------------------\n");

    char abs_path[PATH_MAX];
    if (realpath(input_filename, abs_path) != NULL) {
        fprintf(output_file, "%s\n", abs_path);
    } else {
        fprintf(output_file, "%s\n", input_filename);
    }

    fprintf(output_file, "----------------------------------------\n");

    char buffer[MAX_LINE_LENGTH];
    while (fgets(buffer, MAX_LINE_LENGTH, input_file)) {
        fprintf(output_file, "%s", buffer);
    }

    fprintf(output_file, "\n");
    fclose(input_file);
    fclose(output_file);
}

int main(int argc, char *argv[]) {
    const char *output_filename = DEFAULT_OUTPUT_FILE; // 默认输出文件名
    int opt;

    // 获取当前工作目录
    char cwd[PATH_MAX];
#ifdef _WIN32
    _getcwd(cwd, sizeof(cwd));
#else
    getcwd(cwd, sizeof(cwd));
#endif

    // 存储基础目录路径
    base_dir = strdup(cwd);

    // 解析 .aimignore 文件
    parse_aimignore(cwd);

    while ((opt = getopt(argc, argv, "d:i:")) != -1) {
        switch (opt) {
            case 'd':
                output_filename = optarg; // 指定输出文件名
                break;
            case 'i':
                {
                    // 处理路径，去掉开头的 ./ 
                    char* pattern = optarg;
                    if (strncmp(pattern, "./", 2) == 0) {
                        pattern += 2; // 跳过 "./"
                    }

                    // 检查路径是否以分隔符结尾，表示文件夹
                    int is_dir = 0;
                    size_t len = strlen(pattern);
                    char processed_pattern[MAX_LINE_LENGTH];
                    strcpy(processed_pattern, pattern);
                    
                    if (len > 0 && processed_pattern[len - 1] == PATH_SEPARATOR) {
                        processed_pattern[len - 1] = '\0'; // 去掉尾部的分隔符
                        is_dir = 1;
                    } else {
                        // 如果没有以分隔符结尾，检查是否为目录
                        struct stat statbuf;
                        if (stat(optarg, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
                            is_dir = 1;
                        }
                    }

                    // 跳过空的模式
                    if (strlen(processed_pattern) > 0) {
                        add_ignore_path(processed_pattern, is_dir);
                    }
                }
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-d output_file] [-i ignore_path] <input_file1> <input_file2> ...\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1) {
        fprintf(stderr, "Usage: %s [-d output_file] [-i ignore_path] <input_file1> <input_file2> ...\n", argv[0]);
        return 1;
    }

    FILE *output_file = fopen(output_filename, "w");
    if (!output_file) {
        perror("Error clearing output file");
        return 1;
    }
    
    fprintf(output_file, "当前目录: %s\n", cwd);
    print_tree(cwd, 0, "", 0, output_file); // 确保从当前工作目录开始打印
    fprintf(output_file, "\n");
    
    fclose(output_file);

    for (int i = 0; i < argc; i++) {
        append_file_to_output(argv[i], output_filename);
    }

    free_ignore_list(); // 释放忽略路径链表的内存
    printf("Files merged successfully into %s\n", output_filename);
    return 0;
}
