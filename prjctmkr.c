#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <stat.h>

#define DEFAULT_COMPILE "cc68x -g"

#define FILENAME_SIZE 128 /* (allegedly) maximum path length on Atari TOS */
typedef char filename[FILENAME_SIZE + 1];

#define PREFIX_LENGTH 8    /* 8 chars before the dot */
#define EXTENSION_LENGTH 3 /* 3 chars after */

#define SRC_EXTENSION ".c"
#define OBJ_EXTENSION ".o"

#define PROGRAM_NAME "prjctmkr"
#define PROGRAM_SRC PROGRAM_NAME SRC_EXTENSION
#define own_source(F) (strcmp((char *)(F), PROGRAM_SRC) == 0)

#define COMMAND_LENGTH 512
typedef char command[COMMAND_LENGTH + 1];

#define BUILD_FOLDER "build"
#define TESTS_FOLDER "tests"

#define CURRENT_DIR "."
#define PARENT_DIR ".."
#define DIRECTORY_SEPARATOR '/'
#define EXTENSION_SEPARATOR '.'

#define FIRST_FILE 2

#define HELP 'h'
#define STRUCTURE 's'

#define BUILD 'b'
#define TESTS 't'
#define CLEAN 'c'
#define BUILDALL 'B'
#define TESTSALL 'T'
#define CLEANALL 'C'

#define OP_COUNT 6
const char OPERATIONS[OP_COUNT] = {
	BUILD, BUILDALL,
	TESTS, TESTSALL,
	CLEAN, CLEANALL,
};

#define bool int
#define true 1
#define false 0


void error(const char* e) {
	printf("%s: %s\n", PROGRAM_NAME, e);

	return;
}

bool is_empty(const filename *f)
{
	return (NULL == *((char *)f));
}

bool not_empty(const filename *f)
{
	return (NULL != *((char *)f));
}

bool entry_exists(const filename *entry)
{
	return (access((char *)entry, F_OK) == 0);
}

mode_t get_file_mode(const filename *f)
{
	struct stat file;

	stat((char *)f, &file);

	return file.st_mode;
}

bool is_file(const filename *f)
{
	return S_ISREG(get_file_mode(f));
}

bool is_directory(const filename *f)
{
	return S_ISDIR(get_file_mode(f));
}

/* getcwd() is giving paths like 'dev/c/project' instead of 'c:project' */
void reformat_path(filename *path)
{
	char *from = (char *)path;
	char *to = (char *)path;
	from += 5;

	/* remove 'dev/' */
	while(*from != NULL) *(to++) = *(from++);
	*to = NULL;

	/* 'c/' to 'c:' */
	*((char *)path + 1) = ':';

	return;
}


void copy_filename(filename *_to, filename *_from)
{
	char *to = (char *)_to;
	char *from = (char *)_from;

	int i;
	for(i = 0; (from[i] != NULL && i < FILENAME_SIZE); i++)
		to[i] = from[i];

	to[i] = NULL;

	return;
}

void append_directory(filename *to, const filename *what)
{
	sprintf((char *)to, "%s%c%s",
		(char *)to, DIRECTORY_SEPARATOR, (char *)what);
}


char *get_file_segment(const filename *f, char separator)
{
	if(not_empty(f) && (separator == DIRECTORY_SEPARATOR
					||  separator == EXTENSION_SEPARATOR))
	{
		bool extension_hit = false;
		bool directory_hit = false;

		/* start at the end, work backwards */
		char *curr = (char *)f + strlen(*f) - 1;

		for( ; !directory_hit && curr > (char *)f; curr--)
		{
			if(*curr == EXTENSION_SEPARATOR)
			{
				/* EXTENSION_SEPARATOR *MUST*
				come before DIRECTORY_SEPARATOR */

				if(separator == EXTENSION_SEPARATOR && !directory_hit)
					return curr;

				else
					extension_hit = true;
			}

			else if(*curr == DIRECTORY_SEPARATOR)
			{
				/* DIRECTORY_SEPARATOR *MUST NOT*
				come before EXTENSION_SEPARATOR */

				if(separator == DIRECTORY_SEPARATOR && extension_hit)
					return (curr + 1); /* move to first letter */

				else
					directory_hit = true;
			}
		}
	}

	return (char *)f;
}

char *get_filename(const filename *f)
{
	return get_file_segment(f, DIRECTORY_SEPARATOR);
}

char *get_extension(const filename *f)
{
	return get_file_segment(f, EXTENSION_SEPARATOR);
}

bool has_extension(const filename *f, const char *extension)
{
	return (strcmp(extension, get_extension(f)) == 0);
}

bool is_source(const filename *f)
{
	return has_extension(f, SRC_EXTENSION);
}

bool is_object(const filename *f)
{
	return has_extension(f, OBJ_EXTENSION);
}

void get_prefix(const filename *f, filename *prefix)
{
	char *p = (char *)prefix;

	if(not_empty(f))
	{
		char *file = get_filename(f);
		char *c = file;

		while(c < (file + PREFIX_LENGTH) && *c != EXTENSION_SEPARATOR)
			*p++ = *c++;
	}

	*p = NULL;

	return;
}

void get_directory(const filename *f, filename *directory)
{
	if(is_empty(f))
		*directory = NULL;

	else
	{
		int length;
		char *last_char;
		filename prefix;

		get_prefix(f, &prefix);

		length = strlen(*f) - strlen(prefix);

		copy_filename(directory, f);
	}

	return;
}

bool same_directory(const filename *a, const filename *b)
{
	int limit;
	filename a_dir, b_dir;

	get_directory(a, &a_dir);
	get_directory(b, &b_dir);

	limit = strlen((char *)a_dir);

	if(strlen((char *)b_dir) != limit)
		return false;

	else
	{
		int i;
		for(i = 0; i < limit; i++)
			if(a_dir[i] != b_dir[i]) break;

		return (i == limit);
	}
}

bool sub_directory(const filename *root, const filename *sub)
{
	/* - a directory is a subdirectory of itself
	   - a subdirectory filename will be EQUAL TO OR LONGER
	     than its root directory
	     eg. 'root' => 'root/with/sub/directories'
	*/
	int minimum;
	filename root_dir, sub_dir;

	get_directory(root, &root_dir);
	get_directory(sub, &sub_dir);

	minimum = strlen((char *)root_dir);
	if(strlen((char *)sub_dir) < minimum)
		return false;

	else
	{
		int i;
		for(i = 0; i < minimum; i++)
			if(root_dir[i] != sub_dir[i]) break;

		return (i == minimum);
	}
}


void filter_filenames(filename *files, const char *extension)
{
	filename *file = files;

	while(not_empty(file))
	{
		if(strlen((char *)file) > 2 && has_extension(file, extension))
			file++; /* all good; carry on */

		else
		{
			filename *temp;

			printf("%s [*%s]: rejecting %s\n",
				PROGRAM_NAME, extension, (char *)file);

			/* shift everything above down one */
			for(temp = file; not_empty(temp); temp++)
				copy_filename(temp, temp + 1);

			/* no increment because next file moved down */
		}
	}

	return;
}

void filter_sources(filename *files)
{
	filter_filenames(files, SRC_EXTENSION);

	return;
}

void filter_objects(filename *files)
{
	filter_filenames(files, OBJ_EXTENSION);

	return;
}

void remove_object(const filename *f)
{
	if(entry_exists(f) && is_file(f) && is_object(f))
		remove(*f);

	return;
}


void compile_file(const filename *file, const filename *destination)
{
	if(!entry_exists(file) || !is_file(file))
		printf("%s: not a file: %s\n", PROGRAM_NAME, file);

	/* doesn't compile itself */
	else if(own_source(file))
		printf("%s: [%s] ignoring own source\n", PROGRAM_NAME, file);

	else
	{
		filename prefix, object;
		command compile_command;

		char *compile = getenv("PRJCTMKR")
					  ? getenv("PRJCTMKR") : DEFAULT_COMPILE;

		get_prefix(file, &prefix);

		sprintf((char *)&object, "%s%c%s%s",
			(char *)destination, DIRECTORY_SEPARATOR, (char *)prefix, OBJ_EXTENSION);

		sprintf((char *)&compile_command, "%s -c %s -o %s",
				 compile, (char *)file, (char *)&object);

		printf("    %s\n", compile_command);
		system(compile_command);
	}

	return;
}

void compile_to_folder(filename *files, const filename *destination)
{
	filename *f;

	filter_sources(files);

	for(f = files; not_empty(f); f++)
		compile_file(f, destination);

	return;
}

void clean_folder(const filename *folder)
{
	DIR *d = opendir((char *)folder);

	if(d)
	{
		struct dirent *dir;

		filename cwd, entry;
		getcwd((char *)&cwd, FILENAME_SIZE);

		chdir(*folder);

		while((dir = readdir(d)) != NULL)
		{
			copy_filename(&entry, &dir->d_name);

			if(is_object(&entry)) remove_object(&entry);
		}

		chdir(cwd);

		closedir(d);
	}

	return;
}


void traverse_and_build(filename const *current_directory, const filename *build_directory)
{
	if(!sub_directory((filename *)TESTS_FOLDER, current_directory))
	{
		DIR *d = opendir(*current_directory);

		if(d)
		{
			struct dirent *dir;
			filename entry;

			chdir(*current_directory);

			while((dir = readdir(d)) != NULL)
			{
				copy_filename(&entry, &dir->d_name);

				if(is_source(&entry))
					compile_file(&entry, build_directory);

				else if(is_directory(&entry))
					traverse_and_build(&entry, build_directory);
			}

			chdir(PARENT_DIR);

			closedir(d);
		}
	}

	return;
}


void build(filename *files, const filename* root)
{
	if(is_empty(files))
		error("operation 'build' expects at least 1 file");

	else
	{
		filename build_folder;

		copy_filename(&build_folder, root);
		append_directory(&build_folder, (filename *)BUILD_FOLDER);

		compile_to_folder(files, &build_folder);
	}

	return;
}

void tests(filename *files, const filename *root)
{
	if(is_empty(files))
		error("operation 'tests' expects at least 1 file");

	else
	{
		filename tests_folder;

		chdir(TESTS_FOLDER);

		copy_filename(&tests_folder, root);
		append_directory(&tests_folder, (filename *)TESTS_FOLDER);

		compile_to_folder(files, &tests_folder);

		chdir(PARENT_DIR);
	}

	return;
}

void clean(filename *files, const filename* root)
{
	if(is_empty(files))
		error("operation 'clean' expects at least 1 file");

	else
	{
		filename *f;
		filename build_folder, tests_folder;

		filter_objects(files);

		copy_filename(&build_folder, root);
		append_directory(&build_folder, (filename *)BUILD_FOLDER);

		copy_filename(&tests_folder, root);
		append_directory(&tests_folder, (filename *)TESTS_FOLDER);

		for(f = files; not_empty(f); f++)
		{
			filename build, test;

			copy_filename(&build, &build_folder);
			append_directory(&build, f);

			copy_filename(&test, &tests_folder);
			append_directory(&test, f);

			remove_object(&build);
			remove_object(&test);
		}
	}

	return;
}

void buildall(filename *files, const filename *root)
{
	if(not_empty(files))
		error("operation 'buildall' expects 0 files");

	else
	{
		filename build_directory;

		copy_filename(&build_directory, root);

		append_directory(&build_directory, (filename *)BUILD_FOLDER);

		traverse_and_build(root, &build_directory);

		chdir((char *)root);
	}

	return;
}

void testsall(filename *files, const filename *root)
{
	if(not_empty(files))
		error("operation 'testsall' expects 0 files");

	else
	{
		DIR *d = opendir((char *)TESTS_FOLDER);

		if(d)
		{
			struct dirent *dir;
			filename entry;

			chdir((char *)TESTS_FOLDER);

			while((dir = readdir(d)) != NULL)
			{
				copy_filename(&entry, (filename *)&dir->d_name);

				if(is_source(&entry))
					compile_file(&entry, (filename *)CURRENT_DIR);
			}

			chdir(PARENT_DIR);

			closedir(d);
		}
	}

	return;
}

void cleanall(filename *files, const filename *root)
{
	if(not_empty(files))
		error("operation 'cleanall' expects 0 files");

	else
	{
		clean_folder((filename *)BUILD_FOLDER);
		clean_folder((filename *)TESTS_FOLDER);
	}

	return;
}

void help()
{
	printf("\n  %s:\n", PROGRAM_NAME);
	printf("    C-compiling utility meant to be used with 'make'\n");
	printf("    for managing multiple .c files in nested folders\n");

	printf("\n  usage: %s [%c%c%c%c%c%c%c%c] files.c go.c here.o n.o commas.c ...\n",
		PROGRAM_NAME,
		BUILD, TESTS, CLEAN,
		BUILDALL, TESTSALL, CLEANALL,
		HELP, STRUCTURE);

	printf("\n  operations:\n");

	printf("    [%c] build: compiles any provided .c files into ./%s/*.o\n",
		BUILD, BUILD_FOLDER);

	printf("    [%c] tests: compiles any provided .c files into ./%s/*.o\n",
		TESTS, TESTS_FOLDER);

	printf("    [%c] clean: deletes any provided .o files in ./%s and ./%s\n",
		CLEAN, BUILD_FOLDER, TESTS_FOLDER);

	printf("    [%c] buildall: compiles all .c files outside of ./%s into ./%s/*.o\n",
		BUILDALL, TESTS_FOLDER, BUILD_FOLDER);

	printf("    [%c] testsall: compiles all .c files inside of ./%s into ./%s/*.o\n",
		TESTSALL, TESTS_FOLDER, TESTS_FOLDER);

	printf("    [%c] cleanall: deletes all .o files in both ./%s and ./%s\n",
		CLEANALL, BUILD_FOLDER, TESTS_FOLDER);

	printf("    [%c] help: displays this help text\n",
		HELP);

	printf("    [%c] structure: displays an example project struture\n",
		STRUCTURE);

	printf("\n  notes:\n");
	printf("    - compile command is set in the shell using 'setenv'\n");
	printf("      eg. setenv PRJCTMKR 'cc68x -g' (default: '%s')\n", DEFAULT_COMPILE);
	printf("    - use '/' to separate directories (eg. project/src)\n");
	printf("    - use relative paths for 'build' (eg. src.c folder/tsd.c)\n");
	printf("    - will only delete .o files in %s and %s\n", BUILD_FOLDER, TESTS_FOLDER);
	printf("    - wildcard arguments (eg. *.c) work but are largely untested\n");

	return;
}

void structure()
{
	printf("\n");
	printf("   ./project\n");
	printf("   |> %s\n", PROGRAM_NAME);
	printf("   |> makefile\n");
	printf("   |> main.c\n");
	printf("   |> ./source\n");
	printf("      |> source.c\n");
	printf("      |> ./sub_folder_c\n");
	printf("         |> sub.c\n");
	printf("   |> ./library\n");
	printf("      |> library.c\n");
	printf("   |> %s\n", BUILD_FOLDER);
	printf("      |> main.o\n");
	printf("      |> source.o\n");
	printf("      |> sub.o\n");
	printf("      |> library.o\n");
	printf("   |> %s\n", TESTS_FOLDER);
	printf("      |> test.c\n");
	printf("      |> test.o\n");

	return;
}

void answer(char request)
{
	switch(request)
	{
	case STRUCTURE:
	case (STRUCTURE - ' '): /* to uppercase */
		structure();
		break;

	default:
		help();
	};

	return;
}

bool not_operation(char request)
{
	bool exists = false;

	int i;
	for(i = 0; !exists && i < OP_COUNT; i++)
		if (OPERATIONS[i] == request) exists = true;

	return !exists;
}

void run_operation(char operation, filename *files)
{
	filename root;

	getcwd((char *)root, FILENAME_SIZE);

	reformat_path(&root);

	switch(operation)
	{
	case BUILD:
		build(files, &root);
		break;

	case TESTS:
		tests(files, &root);
		break;

	case CLEAN:
		clean(files, &root);
		break;

	case BUILDALL:
		buildall(files, &root);
		break;

	case TESTSALL:
		testsall(files, &root);
		break;

	case CLEANALL:
		cleanall(files, &root);
		break;
	}

	return;
}


int main(int argc, char *argv[])
{
	if(argc < 2 || strlen(argv[1]) != 1)
		answer(HELP);

	else
	{
		char request = *(argv[1]);

		if(not_operation(request))
			answer(request);

		else
		{
			int number_of_files = argc - FIRST_FILE;
			int buffer_size = number_of_files + 1;

			filename *files = calloc(buffer_size, sizeof(filename));

			if(!files)
				error("could not allocate space for filenames");

			else
			{
				int i;
				filename *f;

				for(i = 0, f = files; i < number_of_files; i++, f++)
					copy_filename(f, (filename *)argv[FIRST_FILE + i]);

				run_operation(request, files);

				free(files);
			}
		}
	}

	return 0;
}
