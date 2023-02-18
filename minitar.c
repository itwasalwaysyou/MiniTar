#include <fcntl.h>
#include <grp.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "minitar.h"

#define NUM_TRAILING_BLOCKS 2
#define MAX_MSG_LEN 512

/*
 * Helper function to compute the checksum of a tar header block
 * Performs a simple sum over all bytes in the header in accordance with POSIX
 * standard for tar file structure.
 */
void compute_checksum(tar_header *header) {
    // Have to initially set header's checksum to "all blanks"
    memset(header->chksum, ' ', 8);
    unsigned sum = 0;
    char *bytes = (char *)header;
    for (int i = 0; i < sizeof(tar_header); i++) {
        sum += bytes[i];
    }
    snprintf(header->chksum, 8, "%07o", sum);
}

/*
 * Populates a tar header block pointed to by 'header' with metadata about
 * the file identified by 'file_name'.
 * Returns 0 on success or -1 if an error occurs
 */
int fill_tar_header(tar_header *header, const char *file_name) {
    memset(header, 0, sizeof(tar_header));
    char err_msg[MAX_MSG_LEN];
    struct stat stat_buf;
    // stat is a system call to inspect file metadata
    if (stat(file_name, &stat_buf) != 0) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to stat file %s", file_name);
        perror(err_msg);
        return -1;
    }

    strncpy(header->name, file_name, 100); // Name of the file, null-terminated string
    snprintf(header->mode, 8, "%07o", stat_buf.st_mode & 07777); // Permissions for file, 0-padded octal

    snprintf(header->uid, 8, "%07o", stat_buf.st_uid); // Owner ID of the file, 0-padded octal
    struct passwd *pwd = getpwuid(stat_buf.st_uid); // Look up name corresponding to owner ID
    if (pwd == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up owner name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->uname, pwd->pw_name, 32); // Owner  name of the file, null-terminated string

    snprintf(header->gid, 8, "%07o", stat_buf.st_gid); // Group ID of the file, 0-padded octal
    struct group *grp = getgrgid(stat_buf.st_gid); // Look up name corresponding to group ID
    if (grp == NULL) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to look up group name of file %s", file_name);
        perror(err_msg);
        return -1;
    }
    strncpy(header->gname, grp->gr_name, 32); // Group name of the file, null-terminated string

    snprintf(header->size, 12, "%011o", (unsigned)stat_buf.st_size); // File size, 0-padded octal
    snprintf(header->mtime, 12, "%011o", (unsigned)stat_buf.st_mtime); // Modification time, 0-padded octal
    header->typeflag = REGTYPE; // File type, always regular file in this project
    strncpy(header->magic, MAGIC, 6); // Special, standardized sequence of bytes
    memcpy(header->version, "00", 2); // A bit weird, sidesteps null termination
    snprintf(header->devmajor, 8, "%07o", major(stat_buf.st_dev)); // Major device number, 0-padded octal
    snprintf(header->devminor, 8, "%07o", minor(stat_buf.st_dev)); // Minor device number, 0-padded octal

    compute_checksum(header);
    return 0;
}

/*
 * Removes 'nbytes' bytes from the file identified by 'file_name'
 * Returns 0 upon success, -1 upon error
 * Note: This function uses lower-level I/O syscalls (not stdio), which we'll learn about later
 */
int remove_trailing_bytes(const char *file_name, size_t nbytes) {
    char err_msg[MAX_MSG_LEN];
    // Note: ftruncate does not work with O_APPEND
    int fd = open(file_name, O_WRONLY);
    if (fd == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to open file %s", file_name);
        perror(err_msg);
        return -1;
    }
    //  Seek to end of file - nbytes
    off_t current_pos = lseek(fd, -1 * nbytes, SEEK_END);
    if (current_pos == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to seek in file %s", file_name);
        perror(err_msg);
        close(fd);
        return -1;
    }
    // Remove all contents of file past current position
    if (ftruncate(fd, current_pos) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to truncate file %s", file_name);
        perror(err_msg);
        close(fd);
        return -1;
    }
    if (close(fd) == -1) {
        snprintf(err_msg, MAX_MSG_LEN, "Failed to close file %s", file_name);
        perror(err_msg);
        return -1;
    }
    return 0;
}

int write_single_file_to_archive(FILE *archive, node_t* file_node) {

    // 1. header, buffer preparation

    tar_header header;

    char buffer[BLOCK_SIZE];

    memset(&header, 0, BLOCK_SIZE);

    // 2. fill header and write to archive

    if (fill_tar_header(&header, file_node->name)) {

        perror("Fail tp fill header.");

        return -1;

    }

    fwrite(&header, BLOCK_SIZE, 1, archive);

    if (ferror(archive)) {

        perror("Fail to write head");

        return -1;

    }

    // 3. write content blocks

    memset(buffer, 0, BLOCK_SIZE);

    FILE *fp = fopen(file_node->name, "r");

    if (!fp) {

        perror("Failed to open file");

        fclose(fp);

        return -1;

    }

    int read_num = BLOCK_SIZE;

    while (read_num == BLOCK_SIZE){

        memset(buffer, 0, BLOCK_SIZE);

        read_num = fread(buffer, 1, BLOCK_SIZE, fp); // read -> buffer from curr_file

        if (ferror(fp)) {

            fclose(fp);

            perror("Fail to read");

            return -1;

        }

        if (fwrite(buffer, 1, BLOCK_SIZE, archive) != BLOCK_SIZE) {     //write -> content from buffer to archive

            fclose(fp);

            perror("Error when writing");

            return -1;

        }

        

    }

    return 0;

}


/*

 * Helper function 1 -  to write 2 zero blocks to archive

 */

int write_zero_blocks(FILE *archive) {

    char buffer[BLOCK_SIZE];

    memset(buffer, 0, BLOCK_SIZE);

    fwrite(buffer, BLOCK_SIZE, 1, archive); //block1 :  buffer --> tar- file  

    fwrite(buffer, BLOCK_SIZE, 1, archive); //block2 :  buffer --> tar- file

    if (ferror(archive)) {

        perror("Failed to write footer");

        return -1;

    }

    return 0;

}


/*

helper Function for :  create and append 

* write: files -> archive (tar) 
*
*1.write the current file to archive(tar), 
*2.move the pointer(file_list) to the next one
*3.iterate
*/


int savetToTar(FILE* archive, const file_list_t* currFile){


    node_t* current = currFile->head;

    while (current != NULL) {

        if (write_single_file_to_archive(archive, current)) {

            fclose(archive);

            return -1;

        }

        current = current->next;

    }
    // Add footer

    if (write_zero_blocks(archive)) {

        fclose(archive);

        return -1;

    }

    return 0;


}


//write file --> tar
//create a new archive(tar)


int create_archive(const char *archive_name, const file_list_t *files) {

    FILE *archive = fopen(archive_name, "w");

    if (!archive) {

        fclose(archive);

        perror("Failed to open archive");

        return -1;

    }
    savetToTar(archive, files);

    fclose(archive);

    return 0;

}


// write file --> tar 
//add an new file to the end of archive(tar)

int append_files_to_archive(const char *archive_name, const file_list_t *files) {

    // 1) check archive_name exist

    FILE *archive;

    if (!(archive = fopen(archive_name, "r+"))) {

        perror("Failed to open archive");

        fclose(archive);

        return -1;

    }

    // 2) remove the footer

    if (remove_trailing_bytes(archive_name, BLOCK_SIZE*NUM_TRAILING_BLOCKS)) {

        perror("Fail to remove the footer!");

        fclose(archive);

    }


    // 3) append to end

    fseek(archive, 0, SEEK_END);

    savetToTar(archive,files); //write files -> archive(tar)

    fclose(archive);

    return 0;

}



// helper function for list and extract

// archieve --> file --> (linked list) 
// mode 1: list
// mode 2: extract-> delecte the file when it duplicate 


int readFromTar(FILE* archive, file_list_t* currFiles, int mode){

    //get first header: 

    fseek(archive, 0, SEEK_SET);


    char buffer[BLOCK_SIZE];

    memset(buffer, 0, BLOCK_SIZE);

    fread(buffer, 1, BLOCK_SIZE, archive); //archive[512] -> buffer

    int size;
    int block_num;

    while (buffer[0]!='\0') { //check if at the end of file

        // 1 header: char -> struct

        if (ferror(archive)) {

            perror("Failed to read archive");

            return -1;

        }

        tar_header* header;

        header = (tar_header *) buffer;


        sscanf(header->size, "%011o", &size);
     
        if(size%BLOCK_SIZE==0){

            block_num = size / BLOCK_SIZE;

        }else{

            block_num = size / BLOCK_SIZE+1;

        }
        if(mode==1){
            if(file_list_add(currFiles, header->name)){
                perror("Fail");
                return -1;

            }

        }else{
                
                    FILE *current_file;
                    if (!(current_file = fopen(header->name, "w+"))) {
                        perror("Failed to open archive");
                        fclose(current_file);
                        return -1;
                    }

                    fread(buffer, size,1, archive); 
                    fwrite(buffer, size,1, current_file);

                    //fread archive --> buffer 
                    // buffer -> file
                    fclose(current_file);
            }




        fseek(archive, block_num * BLOCK_SIZE, SEEK_CUR);

        memset(buffer, 0, BLOCK_SIZE);

        fread(buffer, 1, BLOCK_SIZE, archive);
    }

    return 0;

}


// interate : archive -> list -> header.name 


int get_archive_file_list(const char *archive_name, file_list_t *files){


    // 1) check archive_name exist

    FILE *archive;
    if (!(archive = fopen(archive_name, "r"))) {

        perror("Failed to open archive");

        fclose(archive);

        return -1;

    } 

    fclose(archive);

  
    return 0;
}


/*

int get_archive_file_list(const char *archive_name, file_list_t *files){


    // 1) check archive_name exist

    FILE *archive;
    if (!(archive = fopen(archive_name, "r"))) {

        perror("Failed to open archive");

        fclose(archive);

        return -1;

    } 


    // 2) iterate and get info: archive --> linked list

    readFromTar(archive,files,1); //read archive(tar) -> file

    fclose(archive);


    return 0;
}



*/



int extract_files_from_archive(const char *archive_name) {

    // 1) check archive_name exist

    FILE *archive;
    if (!(archive = fopen(archive_name, "r"))) {

        perror("Failed to open archive");

        fclose(archive);

        return -1;

    }

    /*
    // 2) iterate and get info: archive --> linked list

    file_list_t currFiles;
    file_list_init(&currFiles);
    readFromTar(archive,&currFiles,2); //write down currFiles(gernarate linked list) <-- archive(tar)

     */  
    fclose(archive);
 



    return 0;
}



/*

int extract_files_from_archive(const char *archive_name) {

    // 1) check archive_name exist

    FILE *archive;
    if (!(archive = fopen(archive_name, "r+"))) {

        perror("Failed to open archive");

        fclose(archive);

        return -1;

    }

    // 2) iterate and get info: archive --> linked list

    file_list_t currFiles;
    file_list_init(&currFiles);
    readFromTar(archive,&currFiles,2); //write down currFiles(gernarate linked list) <-- archive(tar)


    return 0;
}


*/