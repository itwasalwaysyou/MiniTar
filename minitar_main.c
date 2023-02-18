#include <stdio.h>

#include <string.h>


#include "file_list.h"

#include "minitar.h"


/* 

    Helper function : build_file_list

    return 0: success

    return 1: fail

*/

int build_file_list(int argc, char **argv, file_list_t *file_list) {

    for (int i = 4; i < argc; i ++){

        if (file_list_add(file_list, argv[i])) {

            return -1;

        }

    }

    return 0;

}


int main(int argc, char **argv) {

    if (argc < 4) {

        printf("Usage: %s -c|a|t|u|x -f ARCHIVE [FILE...]\n", argv[0]);

        return 0;

    }


    file_list_t files;

    file_list_init(&files);


    // TODO: Parse command-line arguments and invoke functions from 'minitar.h'

    // to execute archive operations

    int result = build_file_list(argc, argv, &files);

    if (result) {

        file_list_clear(&files);

        perror("Error: add to file list error with file");

        return -1;

    }


    /*

        parsing process

        

    */

    

    if (strcmp(argv[2], "-f")!= 0) {

        perror("Missing -f");

        return 1;

    }


    if (strcmp(argv[1], "-c") == 0) {

        // Create new archive named argv[2]

        if (argc < 5) {

            file_list_clear(&files);

            perror("Error: Missing arguments");

            return -1;

        }

        if (create_archive(argv[3], &files)!=0) {

            file_list_clear(&files);

            perror("Error: Fail to create archive named");

            return -1;

        }

    }


    else if (strcmp(argv[1], "-a") == 0) {

        // append

        if (argc < 5) {

            perror("Error: Missing arguments");

            file_list_clear(&files);

            return -1;

        }

        if (append_files_to_archive(argv[3], &files)!=0) {

            perror("Error: Fail to append to archive");

            file_list_clear(&files);

            return -1;

        }

    }


    else if (strcmp(argv[1], "-t") == 0) {

        // print files name

        if (argc != 4) {

            perror("Error: incorrect input");

            file_list_clear(&files);

            return 1;

        }

        if (get_archive_file_list(argv[3], &files)) {

            perror("Error: Fail to load file list");

            file_list_clear(&files);

            return 1;

        }

        node_t* node = files.head;

        while (node) {

            printf("%s\n", node->name);

            node = node->next;

        }

    }


    else if (strcmp(argv[1], "-u") == 0) {

        // updata: check subset and append new files to the end of files

        if (argc < 5) {

            perror("Error: Missing arguments");

        }

        file_list_t original_arch;

        file_list_init(&original_arch);

        if (get_archive_file_list(argv[3], &original_arch)) {

            perror("Fail to get files list");

            file_list_clear(&files);

            file_list_clear(&original_arch);

            return -1;

        }

        

        if (file_list_is_subset(&files, &original_arch)) {

            if (append_files_to_archive(argv[3], &files)) {

                perror("Error: Fail to append to archive");

                file_list_clear(&files);

                file_list_clear(&original_arch);


                return 1;

            }

            else {

                file_list_clear(&original_arch);


            }

        } else {

            file_list_clear(&files);

            file_list_clear(&original_arch);

            fprintf(stderr, "Error: One or more of the specified files is not already present in archive");

            // perror("Error: One or more of the specified files is not already present in archive");

            return -1;

        }

    }

    

    else if (strcmp(argv[1], "-x") == 0) {

        // extract files from tar

        if (extract_files_from_archive(argv[3])) {

            perror("Error: Fail to extract files");

            file_list_clear(&files);

            return -1;

        }

    }


    else {

        perror("Error: unknown operation");

    }



    file_list_clear(&files);

    return 0;

}



