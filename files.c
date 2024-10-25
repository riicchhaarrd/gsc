// Because of case sensitivity, we have to iterate through all .gsc files and read the filenames and store them
// lowercase incase of a lookup.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define ALLOCATOR_MALLOC_WRAPPER
#include <core/allocator.h>
#include <core/ds/hash_trie.h>
// int get_memory_usage_kb()
// {
// #ifdef EMSCRIPTEN
// 	return -1;
// #else
// 	FILE *file = fopen("/proc/self/status", "r");
// 	if(!file)
// 	{
// 		return -1;
// 	}
// 	int memory_usage_kb = -1;
// 	char line[256];
// 	while(fgets(line, sizeof(line), file))
// 	{
// 		if(strncmp(line, "VmRSS:", 6) == 0)
// 		{
// 			sscanf(line, "VmRSS: %d kB", &memory_usage_kb);
// 			break;
// 		}
// 	}
// 	fclose(file);
// 	return memory_usage_kb;
// #endif
// }
static HashTrie files;

FILE *open_file(const char *path);
void find_gsc_files(const char *dir_path, int depth);

FILE *open_file(const char *path)
{
	HashTrieNode *n = hash_trie_upsert(&files, path, NULL, false);
    if(!n)
		return NULL;
	return fopen(n->key, "r");
}

void find_gsc_files(const char *dir_path, int depth)
{
	if(depth == 0)
	{
		hash_trie_init(&files);
	}
	struct dirent *entry;
	struct stat file_stat;
	DIR *dir = opendir(dir_path);
	if(dir == NULL)
	{
		perror("Unable to open directory");
		return;
	}
	while((entry = readdir(dir)) != NULL)
	{
		char full_path[1024];
		if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
		{
			continue;
		}
		snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);
		if(stat(full_path, &file_stat) == -1)
		{
			perror("stat");
			continue;
		}
		if(S_ISDIR(file_stat.st_mode))
		{
			find_gsc_files(full_path, depth + 1);
		}
		else
		{
			if(strstr(entry->d_name, ".gsc") != NULL)
			{
				hash_trie_upsert(&files, full_path, &malloc_allocator, false);
				// printf("%s\n", full_path);
			}
		}
	}
	closedir(dir);
}
// int main(int argc, char *argv[])
// {
// 	if(argc < 2)
// 	{
// 		fprintf(stderr, "Usage: %s <directory>\n", argv[0]);
// 		return 1;
// 	}
// 	find_gsc_files(argv[1], 0);
// 	printf("%f MB\n", get_memory_usage_kb() / 1000.f);
// 	for(HashTrieNode *it = files.head; it; it = it->next)
// 	{
// 		printf("%s\n", it->key);
// 		getchar();
// 	}
// 	return 0;
// }
