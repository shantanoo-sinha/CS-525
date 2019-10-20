//
//  buffer_mgr.c
//  buffer_manager
//
//  Created by Deepak Kumar Yadav; Shantanoo Sinha; Nirav Soni on 10/6/19.
//  Copyright © 2019 DeepakKumarYadav. All rights reserved.
//
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "storage_mgr.h"
#include "dberror.h"
#include "buffer_mgr.h"

// Define constants
#define MAX_NO_OF_PAGES 20000
#define MAX_NO_OF_FRAMES 200
#define MAX_K_VALUE 10

// Frame Node which will contain one frame of a buffer pool A frame node consisting one page/frame of a buffer pool
typedef struct pageFrameNode{
    int page_number;                // page number of the page
    int frame_number;               // frame number in the frame list
    int is_dirty;                   // Dirty bit indicator 0: Not Dirty 1: Dirty Bit
    int fixed_count;                // fixed count of the page
    int reference;                  // reference bit for every node for STRATEGY: CLOCK
    int page_frequency;             // frequency of the page per client request for STRATEGY: LFU
    char *data;                     // Data
    struct pageFrameNode *next;
    struct pageFrameNode *previous;
}pageFrameNode;

// Frame list with pointers to head and tail node
typedef struct frameList{
    pageFrameNode *head;    // head node  pointer
    pageFrameNode *tail;    // tail node pointer
}frameList;

// Data per buffer pool
typedef struct bmInfo{
    int number_of_frames;                   // Number of frames
    int read;                               // total no. of read's done
    int write;                              // total no. of write's done
    int pinning_done;                       // total no. of pinning's done
    void *stratData;
    int page_to_frame[MAX_NO_OF_PAGES];     // Array for page_number to frame_number
    int frame_to_page[MAX_NO_OF_FRAMES];    // Array from frame_number to page_number
    int page_to_frequency[MAX_NO_OF_PAGES]; // Array mappping page_number to page_frequency
    bool dirty_flags[MAX_NO_OF_FRAMES];     // Dirty flags of all frames.
    int fixed_count[MAX_NO_OF_FRAMES];      // Fixed count of all frames.
    int khist[MAX_NO_OF_PAGES][MAX_K_VALUE];      // History of reference for each page in memory
    frameList *frames;                      // Pointer to frame list
}bmInfo;

// Creating a new node for frame list
pageFrameNode *newNode(){
    pageFrameNode *node = malloc(sizeof(pageFrameNode));
    node -> page_number = NO_PAGE;
    node -> frame_number = 0;
    node -> is_dirty = 0;
    node -> fixed_count = 0;
    node -> reference = 0;
    node -> page_frequency = 0;
    node -> data =  calloc(PAGE_SIZE, sizeof(SM_PageHandle));
    node -> next = NULL;
    node -> previous = NULL;
    
    // Return the new node
    return node;
}


/*************************************
*
* Method:        updateNodeAsHead
*
* FullName:      updateNodeAsHead
*
* Description:   Make the given node as head node
*
* Access:        public 
*
* Parameter:     frameList * * list
* Parameter:     pageFrameNode * updateNode
*
* Returns:       void
*
*************************************/
void updateNodeAsHead(frameList **list, pageFrameNode *updateNode){
    
    pageFrameNode *head = (*list) -> head;
    
    if( head == NULL || updateNode == (*list) -> head || updateNode == NULL){
        return;
    }
    else if(updateNode == (*list) -> tail){
        pageFrameNode *temp = ((*list) -> tail) -> previous;
        temp -> next = NULL;
        (*list) -> tail = temp;
    }
    else{
        updateNode -> previous -> next = updateNode -> next;
        updateNode -> next -> previous = updateNode -> previous;
    }
    
    updateNode -> next = head;
    head -> previous = updateNode;
    updateNode -> previous = NULL;
    
    (*list) -> head = updateNode;
    (*list) -> head -> previous = NULL;
    return;
}

/*************************************
*
* Method:        findNodeByPageNum
*
* FullName:      findNodeByPageNum
*
* Description:   Find the node with given page number
*
* Access:        public 
*
* Parameter:     frameList * list
* Parameter:     const PageNumber pageNum
*
* Returns:       pageFrameNode *
*
*************************************/
pageFrameNode *findNodeByPageNum(frameList *list, const PageNumber pageNum){
    pageFrameNode *current = list -> head;
    while(current != NULL){
        if(current -> page_number == pageNum)
            return current;
        current = current -> next;
    }
    return NULL;
}

/*************************************
*
* Method:        pageInMemory
*
* FullName:      pageInMemory
*
* Description:   This method checks the page in memory
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     BM_PageHandle * const page
* Parameter:     const PageNumber pageNum
*
* Returns:       pageFrameNode *
*
*************************************/
pageFrameNode *pageInMemory(BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum){
    
    pageFrameNode *found;
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    
    if((info -> page_to_frame)[pageNum] != NO_PAGE){
        if((found = findNodeByPageNum(info -> frames, pageNum)) == NULL)
            return NULL;
        
        // Provide client with data and details of page
        page -> pageNum = pageNum;
        page -> data = found -> data;
        
        // Increase the fixed count and the read-count
        found -> fixed_count++;
        found -> reference = 1;
        
        return found;
    }
    return NULL;
}

/*************************************
*
* Method:        updateNewFrame
*
* FullName:      updateNewFrame
*
* Description:   This method updates the new frame
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     pageFrameNode * found
* Parameter:     BM_PageHandle * const page
* Parameter:     const PageNumber pageNum
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC updateNewFrame(BM_BufferPool *const bm, pageFrameNode *found, BM_PageHandle *const page, const PageNumber pageNum){
    
    SM_FileHandle file_handle;
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    RC status;
    
    if ((status = openPageFile ((char *)(bm -> pageFile), &file_handle)) != RC_OK)
        return status;
    
    /* If the frame to be replaced is dirty, write it back to the disk.*/
    if(found -> is_dirty == 1){
        if((status = ensureCapacity(pageNum, &file_handle)) != RC_OK)
            return status;
        if((status = writeBlock(found -> page_number, &file_handle, found -> data)) != RC_OK)
            return status;
        (info -> write)++;
    }
    
    /* Update the pageToFrame lookup, set the replaceable page's value to NO_PAGE.*/
    (info -> page_to_frame)[found -> page_number] = NO_PAGE;
    
    /* Read the data into new frame.*/
    if((status = ensureCapacity(pageNum, &file_handle)) != RC_OK)
        return status;
    
    if((status = readBlock(pageNum, &file_handle, found -> data)) != RC_OK)
        return status;
    
    /* provide the client with the data and details of page*/
    page -> pageNum = pageNum;
    page -> data = found -> data;
    
    (info -> read)++;
    
    /* Set all the parameters of the new frame, and update the lookup arrays.*/
    found -> is_dirty = 0;
    found -> fixed_count = 1;
    found -> page_number = pageNum;
    found -> reference = 1;
    
    (info -> page_to_frame)[found -> page_number] = found -> frame_number;
    (info -> frame_to_page)[found -> frame_number] = found -> page_number;
    
    closePageFile(&file_handle);
    
    return RC_OK;
    
}

/*************************************
* 
* The page replacement strategies 
*
*************************************/

/*************************************
*
* Method:        pinPage_FIFO
*
* FullName:      pinPage_FIFO
*
* Description:   pins the page with page number pageNum. FIFO implementation
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     BM_PageHandle * const page
* Parameter:     const PageNumber pageNum
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC pinPage_FIFO (BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum) {
    pageFrameNode *found;
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    
    // Check if page is already in memory. The pageToFrame array provides the fast lookup
    if((found = pageInMemory(bm, page, pageNum)) != NULL)
        return RC_OK;
    
    /* If need to load the page*/
    
    // If the frames in the memory are less than the total available frames, find out the first free frame from the head
    if((info -> number_of_frames) < bm -> numPages){
        found = info -> frames -> head;
        int i = 0;
        
        while(i < info -> number_of_frames){
            found = found -> next;
            ++i;
        }
        
        // This frame will be filled up now, so increase the frame count
        (info -> number_of_frames)++;
        updateNodeAsHead(&(info -> frames), found);
    }
    else{
        // For new page, if all the frames are filled, find the oldest frame with fix count 0
        found = info -> frames -> tail;
        
        while(found != NULL && found -> fixed_count != 0)
            found = found -> previous;
        
        if (found == NULL)
            return RC_NO_MORE_SPACE_IN_BUFFER;
        
        updateNodeAsHead(&(info -> frames), found);
    }
    
    RC status;
    if((status = updateNewFrame(bm, found, page, pageNum)) != RC_OK)
        return status;
    
    return RC_OK;
}

/*************************************
*
* Method:        pinPage_LRU
*
* FullName:      pinPage_LRU
*
* Description:   This method pins the page with page number pageNum. This is the LRU implementation
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     BM_PageHandle * const page
* Parameter:     const PageNumber pageNum
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC pinPage_LRU (BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum) {
    pageFrameNode *found;
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    
    // Check if page is already in memory. The pageToFrame array provides the fast lookup
    if((found = pageInMemory(bm, page, pageNum)) != NULL){
        // Put this frame to the head of the frame list, because it is the latest used frame
        updateNodeAsHead(&(info -> frames), found);
        return RC_OK;
    }
    
    /* If need to load the page*/
    
    // If the frames in the memory are less than the total available frames, find out the first free frame from the head
    if((info -> number_of_frames) < bm -> numPages){
        found = info -> frames -> head;
        
        int i = 0;
        while(i < info -> number_of_frames){
            found = found -> next;
            ++i;
        }
        //This frame will be filled up now, so increase the frame count
        (info -> number_of_frames)++;
    }
    else{
        // For new page, if all the frames are filled, then try to find a frame with fixed count 0 starting from the tail
        found = info -> frames -> tail;
        
        while(found != NULL && found -> fixed_count != 0)
            found = found -> previous;
        
        // If reached to head, it means no frames with fixed count 0 available
        if (found == NULL)
            return RC_NO_MORE_SPACE_IN_BUFFER;
    }
    
    RC status;
    if((status = updateNewFrame(bm, found, page, pageNum)) != RC_OK)
        return status;
    
    // Put this frame to the head of the frame list, because it is the latest used frame
    updateNodeAsHead(&(info -> frames), found);
    
    return RC_OK;
}

/*************************************
*
* Method:        pinPage_LRU_K
*
* FullName:      pinPage_LRU_K
*
* Description:   This method pins the page with page number pageNum. This is the LRU_K implementation
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     BM_PageHandle * const page
* Parameter:     const PageNumber pageNum
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC pinPage_LRU_K (BM_BufferPool *const bm, BM_PageHandle *const page,const PageNumber pageNum) {
    pageFrameNode *found;
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    int K = (int)(info -> stratData);
    int i;
    (info -> pinning_done)++;
    
    // Check if page is already in memory. The pageToFrame array provides the fast lookup
    if((found = pageInMemory(bm, page, pageNum)) != NULL){
        for(i = K-1; i>0; i--)
            info -> khist[found -> page_number][i] = info -> khist[found -> page_number][i-1];
        info -> khist[found -> page_number][0] = info -> pinning_done;
        
        return RC_OK;
    }
    /* If need to load the page*/
    
    // If the number_of_frames in the memory are less than the total available frames, find out the first free frame from the head
    if((info -> number_of_frames) < bm -> numPages){
        found = info -> frames -> head;
        
        int i = 0;
        while(i < info -> number_of_frames){
            found = found -> next;
            ++i;
        }
        
        // This frame will be filled up now, so increase the frame count
        (info -> number_of_frames)++;
    }
    else{
        // For new page, if all the frames are filled, then try to find a frame with fixed count 0 starting from the tail
        pageFrameNode *current;
        int dist, max_dist = -1;
        
        current = info -> frames -> head;
        
        while(current != NULL){
            if(current -> fixed_count == 0 && info -> khist[current -> page_number][K] != -1){
                
                dist = info -> pinning_done - info -> khist[current -> page_number][K];
                
                if(dist > max_dist){
                    max_dist = dist;
                    found = current;
                }
            }
            current = current -> next;
        }
        
        // If reached to head, it means no frames with fixed count 0 available
        if(max_dist == -1){
            current = info -> frames -> head;
            
            while(current -> fixed_count != 0 && current != NULL){
                dist = info -> pinning_done - info -> khist[current -> page_number][0];
                if(dist > max_dist){
                    max_dist = dist;
                    found = current;
                }
                current = current -> next;
            }
            
            // If reached to head, it means no frames with fixed count 0 available
            if (max_dist == -1)
                return RC_NO_MORE_SPACE_IN_BUFFER;
        }
    }
    
    RC status;
    if((status = updateNewFrame(bm, found, page, pageNum)) != RC_OK)
        return status;
    
    for(i = K-1; i>0; i--){
        info -> khist[found -> page_number][i] = info -> khist[found -> page_number][i-1];
    }
    info -> khist[found -> page_number][0] = info -> pinning_done;
    
    return RC_OK;
}

/*************************************
*
* Method:        initBufferPool
*
* FullName:      initBufferPool
*
* Description:   This method creates a new buffer pool with numPages page frames using the page replacement strategy strategy. The pool is used to cache pages from the page file with name pageFileName.
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     const char * const pageFileName
* Parameter:     const int numPages
* Parameter:     ReplacementStrategy strategy
* Parameter:     void * stratData
*
* Returns:       RC
*
*************************************/
RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, const int numPages, ReplacementStrategy strategy, void *stratData) {
    int i;
    SM_FileHandle file_handle;
    
    if(numPages <= 0)
        return RC_INVALID_BM;
    
    if (openPageFile ((char *)pageFileName, &file_handle) != RC_OK)
        return RC_FILE_NOT_FOUND;

    // Initialize the data for bmInfo
    bmInfo *info = malloc(sizeof(bmInfo));
    
    info -> number_of_frames = 0;
    info -> read = 0;
    info -> write = 0;
    info -> stratData = stratData;
    info -> pinning_done = 0;
    
    // Initialize the lookup arrays with 0 values
    memset(info -> frame_to_page, NO_PAGE, MAX_NO_OF_FRAMES*sizeof(int));
    memset(info -> page_to_frame, NO_PAGE, MAX_NO_OF_PAGES*sizeof(int));
    memset(info -> dirty_flags, NO_PAGE, MAX_NO_OF_FRAMES*sizeof(bool));
    memset(info -> fixed_count, NO_PAGE, MAX_NO_OF_FRAMES*sizeof(int));
    memset(info -> khist, -1, sizeof(&(info -> khist)));
    memset(info -> page_to_frequency, 0, MAX_NO_OF_PAGES*sizeof(int));
    
    // Creating the linked list of empty frames
    info -> frames = malloc(sizeof(frameList));
    
    info -> frames -> head = info -> frames -> tail = newNode();
    
    for(i = 1; i<numPages; ++i){
        info -> frames -> tail -> next = newNode();
        info -> frames -> tail -> next -> previous = info -> frames -> tail;
        info -> frames -> tail = info -> frames -> tail -> next;
        info -> frames -> tail -> frame_number = i;
    }
    
    bm -> numPages = numPages;
    bm -> pageFile = (char*) pageFileName;
    bm -> strategy = strategy;
    bm -> mgmtData = info;
    
    closePageFile(&file_handle);
    
    return RC_OK;
}

/*************************************
*
* Method:        shutdownBufferPool
*
* FullName:      shutdownBufferPool
*
* Description:   This method destroys a buffer pool. If the buffer pool contains any dirty pages, then these pages should be written back to disk before destroying the pool.
*				 This method should free up all resources associated with buffer pool. For example, it should free the memory allocated for page frames. 
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC shutdownBufferPool(BM_BufferPool *const bm) {
    if (!bm || bm -> numPages <= 0)
        return RC_INVALID_BM;

    RC status;
    
    if((status = forceFlushPool(bm)) != RC_OK)
        return status;

    bmInfo *info = (bmInfo *)bm -> mgmtData;
    pageFrameNode *current = info -> frames -> head;
    
    while(current != NULL){
        current = current -> next;
        free(info -> frames -> head -> data);
        free(info -> frames -> head);
        info -> frames -> head = current;
    }
    
    info -> frames -> head = info -> frames -> tail = NULL;
    free(info -> frames);
    free(info);
    
    bm -> numPages = 0;
    
    return RC_OK;
}

/*************************************
*
* Method:        forceFlushPool
*
* FullName:      forceFlushPool
*
* Description:   This method causes all dirty pages (with fix count 0) from the buffer pool to be written to disk
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC forceFlushPool(BM_BufferPool *const bm) {
    if (!bm || bm -> numPages <= 0)
        return RC_INVALID_BM;
    
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    pageFrameNode *current = info -> frames -> head;
    
    SM_FileHandle file_handle;
    
    if (openPageFile ((char *)(bm -> pageFile), &file_handle) != RC_OK)
        return RC_FILE_NOT_FOUND;
    
    while(current != NULL){
        if(current -> is_dirty == 1){
            if(writeBlock(current -> page_number, &file_handle, current -> data) != RC_OK)
                return RC_WRITE_FAILED;
            current -> is_dirty = 0;
            (info -> write)++;
        }
        current = current -> next;
    }
    
    closePageFile(&file_handle);
    
    return RC_OK;
}

/*************************************
*
*
* Page Management Functions
*
*
*************************************/

/*************************************
*
* Method:        markDirty
*
* FullName:      markDirty
*
* Description:   This method marks a page as dirty
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     BM_PageHandle * const page
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page) {
    if (!bm || bm -> numPages <= 0)
        return RC_INVALID_BM;
    
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    pageFrameNode *found;
    
    /* Locate the page to be marked as dirty.*/
    if((found = findNodeByPageNum(info -> frames, page -> pageNum)) == NULL)
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    
    /* Mark the page as dirty */
    found -> is_dirty = 1;
    
    return RC_OK;
}

/*************************************
*
* Method:        unpinPage
*
* FullName:      unpinPage
*
* Description:   This method unpins the page page. The pageNum field of page should be used to figure out which page to unpin
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     BM_PageHandle * const page
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page) {
    if (!bm || bm -> numPages <= 0)
        return RC_INVALID_BM;
    
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    pageFrameNode *found;
    
    // Locate the page to be unpinned
    if((found = findNodeByPageNum(info -> frames, page -> pageNum)) == NULL)
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    
    // Unpinned so decrease the fixed_count by 1
    if(found -> fixed_count > 0){
        found -> fixed_count--;
    }
    else{
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }
    
    return RC_OK;
}

/*************************************
*
* Method:        forcePage
*
* FullName:      forcePage
*
* Description:   This method should write the current content of the page back to the page file on disk
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     BM_PageHandle * const page
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page) {
    if (!bm || bm -> numPages <= 0)
        return RC_INVALID_BM;
    
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    pageFrameNode *found;
    SM_FileHandle file_handle;
    
    if (openPageFile ((char *)(bm -> pageFile), &file_handle) != RC_OK)
        return RC_FILE_NOT_FOUND;
    
    //Locate the page to be forced on the disk
    if((found = findNodeByPageNum(info -> frames, page -> pageNum)) == NULL){
        closePageFile(&file_handle);
        return RC_NON_EXISTING_PAGE_IN_FRAME;
    }
    
    //write the current content of the page back to the page file on disk
    if(writeBlock(found -> page_number, &file_handle, found -> data) != RC_OK){
        closePageFile(&file_handle);
        return RC_WRITE_FAILED;
    }
    
    (info -> write)++;
    
	//closes the page file
    closePageFile(&file_handle);
    
    return  RC_OK;
}


/*************************************
*
* Method:        pinPage
*
* FullName:      pinPage
*
* Description:   This method pins the page with page number pageNum. The buffer manager is responsible to set the pageNum field of the page handle passed to the method. Similarly, the data field should point to the page frame the page is stored in (the area in memory storing the content of the page).
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
* Parameter:     BM_PageHandle * const page
* Parameter:     const PageNumber pageNum
*
* Returns:       RC: Return Code specified in the file "dberror.h"
*
*************************************/
RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, const PageNumber pageNum) {

    if (!bm || bm -> numPages <= 0)
        return RC_INVALID_BM;
    
    if(pageNum < 0)
        return RC_READ_NON_EXISTING_PAGE;
    
    switch (bm -> strategy) {
        case RS_FIFO:   return pinPage_FIFO(bm,page,pageNum);
                        break;
        case RS_LRU:    return pinPage_LRU(bm,page,pageNum);
                        break;
        case RS_LRU_K:  return pinPage_LRU_K(bm,page,pageNum);
                        break;
        default:        return RC_UNKNOWN_STRATEGY;
                        break;
    }
    
    return RC_OK;
}

/*************************************
*
* Method:        getFrameContents
*
* FullName:      getFrameContents
*
* Description:   This method return the value of frame_to_page
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
*
* Returns:       PageNumber *
*
*************************************/
PageNumber *getFrameContents (BM_BufferPool *const bm)  {
    //This method return the value of frame_to_page
    return ((bmInfo *)bm -> mgmtData) -> frame_to_page;
}

/*************************************
*
* Method:        getDirtyFlags
*
* FullName:      getDirtyFlags
*
* Description:   iterates through the list of frames and updates the value of dirty_flags
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
*
* Returns:       bool *
*
*************************************/
bool *getDirtyFlags (BM_BufferPool *const bm) {
    //iterates through the list of frames and updates the value of dirty_flags
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    pageFrameNode *cur = info -> frames -> head;
    
    while (cur != NULL){
        (info -> dirty_flags)[cur -> frame_number] = cur -> is_dirty;
        cur = cur -> next;
    }
    
    return info -> dirty_flags;
}

/*************************************
*
* Method:        getFixCounts
*
* FullName:      getFixCounts
*
* Description:   This method goes through the list of frames and then updates the value of fixed_count
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
*
* Returns:       int *
*
*************************************/
int *getFixCounts (BM_BufferPool *const bm) {
    //This method goes through the list of frames and then updates the value of fixed_count
    bmInfo *info = (bmInfo *)bm -> mgmtData;
    pageFrameNode *cur = info -> frames -> head;
    
    while (cur != NULL){
        (info -> fixed_count)[cur -> frame_number] = cur -> fixed_count;
        cur = cur -> next;
    }
    
    return info -> fixed_count;
}

/*************************************
*
* Method:        getNumReadIO
*
* FullName:      getNumReadIO
*
* Description:   returns the value of read
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
*
* Returns:       int
*
*************************************/
int getNumReadIO (BM_BufferPool *const bm) {
    //returns the value of read
    return ((bmInfo *)bm -> mgmtData) -> read;
}

/*************************************
*
* Method:        getNumWriteIO
*
* FullName:      getNumWriteIO
*
* Description:   returns the value of write
*
* Access:        public 
*
* Parameter:     BM_BufferPool * const bm
*
* Returns:       int
*
*************************************/
int getNumWriteIO (BM_BufferPool *const bm) {
    //returns the value of write
    return ((bmInfo *)bm -> mgmtData) -> write;
}