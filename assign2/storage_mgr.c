//
//  storage_mgr.c
//  storage_manager
//
//  Created by Deepak Kumar Yadav; Shantanoo Sinha; Nirav Soni on 9/11/19.
//  Copyright © 2019 DeepakKumarYadav. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "storage_mgr.h"
#include "dberror.h"

int init = 0;
/**************************************************************************
*
* Method:        initStorageManager
*
* FullName:      initStorageManager
*
* Description:   Initialize storage manager
*
* Access:        public 
*
* Returns:       void
*
**************************************************************************/
void initStorageManager(){
    chdir(BASE_DIR);
    printf("Using %s directory for all operations(Set in storage_mgr.h file)\n", BASE_DIR);
    init = 1;
    printf(" ======== Storage manager initalization: COMPLETE  ======== \n");
}

/**************************************************************************
*
* Method:        createPageFile
*
* FullName:      createPageFile
*
* Description:   Creating a page of size PAGE_SIZE and filling it with \0
*
* Access:        public 
*
* Parameter:     char * fileName
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC createPageFile (char *fileName){
    FILE *page_file = fopen(fileName, "w");
    char *total_page_str, *first_page;

    // allocate "first" page to store total number of pages information
    total_page_str = (char *) calloc(PAGE_SIZE, sizeof(char)); 

    // considered as actual first page for the data 
    first_page = (char *) calloc(PAGE_SIZE, sizeof(char));   

    strcat(total_page_str,"1\n");

    // Write data to file
    fwrite(total_page_str, sizeof(char), PAGE_SIZE, page_file);
    fwrite(first_page, sizeof(char), PAGE_SIZE, page_file);

    // Close page file and free memory
    fclose(page_file);
    free(first_page);
    free(total_page_str);

    return RC_OK;
}

/**************************************************************************
*
* Method:        openPageFile
*
* FullName:      openPageFile
*
* Description:   Open a page file and get file related information
*
* Access:        public 
*
* Parameter:     char * fileName
* Parameter:     SM_FileHandle * fHandle
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC openPageFile (char *fileName, SM_FileHandle *fHandle){
    FILE *page_file = fopen(fileName, "r+");

    if (page_file){

        char *stream;

        stream = (char *) calloc(PAGE_SIZE, sizeof(char));
        fgets(stream, PAGE_SIZE, page_file);

        // Handling new line characters 
        stream = strtok(stream, "\n"); 

        // Updating file handler components
        fHandle->fileName = fileName; 
        fHandle->totalNumPages = atoi(stream);
        fHandle->curPagePos = 0;
        fHandle->mgmtInfo = page_file;

        // Free up memory
        free(stream);

        // Return OK Code
        return RC_OK;
    }
    return RC_FILE_NOT_FOUND;
}

/**************************************************************************
*
* Method:        closePageFile
*
* FullName:      closePageFile
*
* Description:   Close a page file
*
* Access:        public 
*
* Parameter:     SM_FileHandle * fHandle
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC closePageFile (SM_FileHandle *fHandle){
    
    // closing an open file present at fHandle->mgmtInfo
    if(!fclose(fHandle->mgmtInfo)){
        return RC_OK;
     }

    // If file cannnot be closed
    return RC_FILE_NOT_FOUND;
}

/**************************************************************************
*
* Method:        destroyPageFile
*
* FullName:      destroyPageFile
*
* Description:   Destroy a page file
*
* Access:        public 
*
* Parameter:     char * fileName
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC destroyPageFile (char *fileName){

  // Check if file could be destoyed  
  if (!remove(fileName)){
    return RC_OK;
  }

  // If file destroy failed
  return RC_FILE_NOT_FOUND;
}

/**************************************************************************
*
* Method:        readBlock
*
* FullName:      readBlock
*
* Description:   Read a block
*
* Access:        public 
*
* Parameter:     int pageNum
* Parameter:     SM_FileHandle * fHandle
* Parameter:     SM_PageHandle memPage
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC readBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){

    int seek_status;
    size_t read_block_size;

    // Check if a valid page number is available
    if (pageNum > fHandle->totalNumPages || pageNum < 0)
        return RC_READ_NON_EXISTING_PAGE;
    
    // Check if the file pointer is available
    if (fHandle->mgmtInfo == NULL)
        return RC_FILE_NOT_FOUND;

    seek_status = fseek(fHandle->mgmtInfo, (pageNum+1)*PAGE_SIZE*sizeof(char), SEEK_SET);

    // If we were able to seek status was a success read the file page into memory page
    if (seek_status == 0){
        read_block_size = fread(memPage, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);
        fHandle->curPagePos = pageNum;
        return RC_OK;
    }
    else{
        return RC_READ_NON_EXISTING_PAGE;
    }
}

/**************************************************************************
*
* Method:        getBlockPos
*
* FullName:      getBlockPos
*
* Description:   Gets the current page position
*
* Access:        public 
*
* Parameter:     SM_FileHandle *fHandle
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
int getBlockPos (SM_FileHandle *fHandle){
    return fHandle->curPagePos;
}

/**************************************************************************
*
* Method:        readFirstBlock
*
* FullName:      readFirstBlock
*
* Description:   Read first block
*
* Access:        public 
*
* Parameter:     SM_FileHandle * fHandle
* Parameter:     SM_PageHandle memPage
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC readFirstBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(0, fHandle, memPage);
}

/**************************************************************************
*
* Method:        readPreviousBlock
*
* FullName:      readPreviousBlock
*
* Description:   Read previous block
*
* Access:        public 
*
* Parameter:     SM_FileHandle * fHandle
* Parameter:     SM_PageHandle memPage
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC readPreviousBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle->curPagePos-1, fHandle, memPage);
}

/**************************************************************************
*
* Method:        readCurrentBlock
*
* FullName:      readCurrentBlock
*
* Description:   Read current block
*
* Access:        public 
*
* Parameter:     SM_FileHandle * fHandle
* Parameter:     SM_PageHandle memPage
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC readCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle->curPagePos, fHandle, memPage);
}

/**************************************************************************
*
* Method:        readNextBlock
*
* FullName:      readNextBlock
*
* Description:   Into the Page handler,Reads  next page from the file handler 
*
* Access:        public 
*
* Parameter:     SM_FileHandle * fHandle
* Parameter:     SM_PageHandle memPage
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC readNextBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle->curPagePos+1, fHandle, memPage);
}

/**************************************************************************
*
* Method:        readLastBlock
*
* FullName:      readLastBlock
*
* Description:   Into the page handler, Read  last page of the file from the file 
*
* Access:        public 
*
* Parameter:     SM_FileHandle * fHandle
* Parameter:     SM_PageHandle memPage
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC readLastBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return readBlock(fHandle->totalNumPages, fHandle, memPage);
}



/**************************************************************************
*
* Method:        writeBlock
*
* FullName:      writeBlock
*
* Description:   Write a page block to disk using the absolute position
*
* Access:        public 
*
* Parameter:     int pageNum
* Parameter:     SM_FileHandle * fHandle
* Parameter:     SM_PageHandle memPage
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC writeBlock (int pageNum, SM_FileHandle *fHandle, SM_PageHandle memPage){

    int seek_status;
    size_t write_block_size;

    // Checks if given page number is valid or not
    if (pageNum > (fHandle->totalNumPages) || (pageNum < 0))
        return RC_WRITE_FAILED;

    // Seek the pointer to the page no mentioned by the user
    seek_status = fseek(fHandle->mgmtInfo, (pageNum+1)*PAGE_SIZE*sizeof(char), SEEK_SET); 

    if (seek_status == 0){

        // Writes data from memPage to file
        write_block_size = fwrite(memPage, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo); 
        fHandle->curPagePos = pageNum;

        // Send Positve status
        return RC_OK;
    }
    else{
        // Send write failed status 
        return RC_WRITE_FAILED;
    }
}

/**************************************************************************
*
* Method:        writeCurrentBlock
*
* FullName:      writeCurrentBlock
*
* Description:   Write a page block to disk using the current position
*
* Access:        public 
*
* Parameter:     SM_FileHandle * fHandle
* Parameter:     SM_PageHandle memPage
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC writeCurrentBlock (SM_FileHandle *fHandle, SM_PageHandle memPage){
    return writeBlock (fHandle->curPagePos, fHandle, memPage); /* calls writeblock function and passes current page position */
}

/**************************************************************************
*
* Method:        appendEmptyBlock
*
* FullName:      appendEmptyBlock
*
* Description:   Increase the number of pages in the file by one. The new last page should be filled with zero bytes.
*
* Access:        public 
*
* Parameter:     SM_FileHandle * fHandle
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC appendEmptyBlock (SM_FileHandle *fHandle){
    
    int seek_status;
    size_t write_block_size;
    SM_PageHandle page_handle;

    // Get pointer after allocating memory
    page_handle = (char *) calloc(PAGE_SIZE, sizeof(char)); 

    // Seek the pointer to end of the file
    seek_status = fseek(fHandle->mgmtInfo,(fHandle->totalNumPages + 1)*PAGE_SIZE*sizeof(char) , SEEK_END); 

    if (seek_status == 0){

        // write the data(0) from the memory block pointed by eb to the file
        write_block_size = fwrite(page_handle, sizeof(char), PAGE_SIZE, fHandle->mgmtInfo);
        fHandle->totalNumPages = fHandle->totalNumPages + 1;
        fHandle->curPagePos = fHandle->totalNumPages;
        rewind(fHandle->mgmtInfo);

        // updates total number of pages information in the file
        fprintf(fHandle->mgmtInfo, "%d\n" , fHandle->totalNumPages); 
        fseek(fHandle->mgmtInfo, (fHandle->totalNumPages + 1)*PAGE_SIZE*sizeof(char), SEEK_SET);
        
        // Free memory
        free(page_handle);

        // Return success
        return RC_OK;
    }
    else{

        // Free memory
        free(page_handle);

        // Return write failed
        return RC_WRITE_FAILED;
    }
}

/**************************************************************************
*
* Method:        ensureCapacity
*
* FullName:      ensureCapacity
*
* Description:   If the file has less than numberOfPages pages then increase the size to numberOfPages
*
* Access:        public
*
* Parameter:     int numberOfPages
* Parameter:     SM_FileHandle * fHandle
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
**************************************************************************/
RC ensureCapacity (int numberOfPages, SM_FileHandle *fHandle){

    if (fHandle->totalNumPages < numberOfPages){
        // Calculate no of pages that are required
        int numPages = numberOfPages - fHandle->totalNumPages; 
        for ( int i=0; i < numPages; i++){
            // Increase the size by the calculated amount
            appendEmptyBlock(fHandle);
        }
    }
    // Return success
    return RC_OK;
}

