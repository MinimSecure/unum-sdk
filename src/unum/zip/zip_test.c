#include "unum.h"
#ifdef FEATURE_GZIP_REQUESTS // Only if this is feature is enabled
#include "zip.h"

// Add a file to the zip file
// zf - Zipfile descriptor
// filename - File to be added to the zip file
// Returns 0 on success and negative on failure
int add_file_to_zip(zipFile zf, char *filename)
{
    FILE *fin;
    int size_read;
    int zip64 = 0;
    void *buf = NULL;
    int err = 0;
    int size_buf = 16384;
    char *save_name = filename; // Name after leading /'s removed
    zip_fileinfo zi; // File Info for Zipped files

    buf = (void*)UTIL_MALLOC(size_buf);
    if(buf == NULL) {
        printf("Error allocating memory\n");
        return -1;
    }

    // Remove leading slashes from filename
    while(save_name[0] == '/') {
        save_name++;
    }

    zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
    zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
    zi.dosDate = 0;
    zi.internal_fa = 0;
    zi.external_fa = 0;

    err = zipOpenNewFileInZip3_64(zf, save_name, &zi,
                                 NULL, 0, NULL, 0, NULL, Z_DEFLATED,
                                 Z_DEFAULT_COMPRESSION, 0,
                                 /* -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, */
                                 -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                 NULL, 0, zip64);
    if (err != ZIP_OK) {
        printf("%s: Error while opning zip file\n", __func__);
        UTIL_FREE(buf);
        return -2;
    }
    // Open the file to add to the zip
    fin = fopen64(filename, "rb");
    if(fin == NULL) {
        printf("error in opening %s for reading\n", filename);
        UTIL_FREE(buf);
	return -3;
    }

    // Read from the input file
    // And write into the zipped file
    do {
        err = ZIP_OK;
        size_read = (int)fread(buf, 1, size_buf, fin);
        if (size_read < size_buf) {
            if (feof(fin) == 0) {
                printf("error in reading %s\n", filename);
                err = ZIP_ERRNO;
            }

            if (size_read > 0) {
                err = zipWriteInFileInZip(zf, buf, size_read);
                if (err < 0) {
                    printf("error in writing %s in the zipfile\n",
                                                 filename);
                }

            }
        }
    }while((err == ZIP_OK) && (size_read > 0));

    if (fin) {
        fclose(fin);
    }
    // Close zip file descriptor
    err = zipCloseFileInZip(zf);
    if (err != ZIP_OK) {
        printf("error in closing %s in the zipfile\n",
                                    filename);
        UTIL_FREE(buf);
        return -4;
    }
    UTIL_FREE(buf);
    return 0;
}

// Test Zip main function
// Returns 0 in success, negative integer otherwise
int test_zip(void)
{
    char *zip_file = "/tmp/test.zip"; // zipped file name
    struct dirent *dp;
    DIR *dfd;
    char dir[256];
    char filename[512];
    zipFile zf;
    int err = 0;
 
    printf("Enter the directory name\n");
    scanf("%256s", dir);

    if(!util_path_exists(dir)) {
        printf("The directory %s does n't exit\n", dir);
        return -1;
    }

    // Open Directory to be zipped
    if((dfd = opendir(dir)) == NULL) {
        printf("Can't open %s\n", dir);
        return -3;
    }

    // Open the zip file for creation
    zf = zipOpen64(zip_file, 0);
    if(zf == NULL) {
        printf("%s: Error while opening zip file\n", __func__);
        closedir(dfd);
        return -2;
    }

    // Iterate through the direcotry and add all files in the directory 
    // to the zip file
    while((dp = readdir(dfd)) != NULL) {
        // Skip anything that starts w/ .
        if(dp->d_name[0] == '.') {
            continue;
        }
        sprintf(filename, "%s/%s", dir, dp->d_name);
        printf("Adding file: %s to %s\n", filename, zip_file);
        // Add this file to the zip file
        err = add_file_to_zip(zf, filename);
        if (err != 0) {
            printf("%s: Error while adding file: %s\n",__func__, filename);
            break;
        }
    }
    closedir(dfd);

    // Close the zip file descriptor
    err = zipClose(zf, NULL);
    if(err != ZIP_OK) {
        printf("error in closing %s\n", zip_file);
        return -4;
    }

    printf("Zipped file is : %s\n", zip_file);
    return 0;
}

#endif // FEATURE_GZIP_REQUESTS
