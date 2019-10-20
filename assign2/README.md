#   CS-525: Advanced Database Organization
##   Assignment No.2 - Buffer Manager

##   Group #13
* **Shantanoo Sinha** ([ssinha15@hawk.iit.edu](mailto:ssinha15@hawk.iit.edu)) - *A20447312*

* **Nirav Soni** ([nsoni10@hawk.iit.edu](mailto:nsoni10@hawk.iit.edu)) - *A20435538*

* **Deepak Kumar Yadav** ([dyadav3@hawk.iit.edu](mailto:dyadav3@hawk.iit.edu)) - *A20447332*

* * *

#   Project Description
The goal of this assignment is to implement a buffer manager. The buffer manager manages a fixed number 
of pages in memory that represent pages from a page file managed by the storage manager implemented in 
assignment 1. The memory pages managed by the buffer manager are called page frames or frames for short. 
We call the combination of a page file and the page frames storing pages from that file a Buffer Pool. 
The Buffer manager should be able to handle more than one open buffer pool at the same time. However, 
there can only be one buffer pool for each page file. Each buffer pool uses one page replacement 
strategy that is determined when the buffer pool is initialized. We have implemented two replacement 
strategies FIFO and LRU.

* * *

# How To Run The Script #
With default test cases:

Compile :
```make assign2_1```

Run :
```./assign2_1```


With additional test cases:

Compile :
```make assign2_2```

Run :
```./assign2_2```


To clean:

Run :
```make clean```

* * *

# Logic
## Data Structures and Design
Frames is implemented as a doubly-linked list. Each node of the frames is called pageFrameNode. It's attributes are:

* page_number  - the page number of the page in the pageFile.
* frame_number - the number of the frame in the frame list.
* is_dirty     - the dirty bit of the frame.( 1 = dirty, 0 = not dirty).
* fixed_count  - fixCount of the page based on the pinning/un-pinning request.
* reference    - reference bit per node to be used by any page replacement algorithm, if required.
* hist         - history of reference of each page in memory.
* data         - actual data pointed by the frameNode.


Few attributes are required at BufferPool level which is implemented under bmInfo, and assigned it to the BM_BufferPool->mgmtData. The attributes of this structure are :

* number_of_frames - filled number of frames in the frame list.
* read      - total number of read done on the buffer pool.
* write     - total number of write done on the buffer pool.
* pinning_done- total number of pinning done for the buffer pool.
* stratData    - value of BM_BufferPool->stratData
* page_to_frame - an array from pageNumber to frameNumber. Value : FrameNum, if page in memory; otherwise -1.
* frame_to_page - an array from frameNumber to pageNumber. Value : PageNum, if frame is full; otherwise -1.
* dirty_flags - an array of dirty flags of all the frames.
* fixed_count - an array of fixed count of all the frames.

* * *

## Buffer Pool Functions
### initBufferPool
It creates a new buffer pool with numPages page frames using the page replacement strategy strategy. The pool is used to cache pages from the page file with name pageFileName. Initially, all page frames should be empty. The page file should already exist, i.e., this method should not generate a new page file. stratData can be used to pass parameters for the page replacement strategy. For example, for LRU-k this could be the parameter k.

### shutdownBufferPool
It destroys a buffer pool. This method should free up all resources associated with the buffer pool. For example, it should free the memory allocated for page frames. If the buffer pool contains any dirty pages, then these pages should be written back to disk before destroying the pool. It is an error to shutdown a buffer pool that has pinned pages.

### forceFlushPool
It causes all dirty pages (with fix count 0) from the buffer pool to be written to disk

* * *

## Page Management Functions
### pinPage
It pins the page with page number pageNum. The buffer manager is responsible to set the pageNum field of the page handle passed to the method. Similarly, the data field should point to the page frame the page is stored in (the area in memory storing the content of the page).

### unpinPage
It unpins the page page. The pageNum field of page should be used to figure out which page to unpin.

### markDirty
It marks a page as dirty. It iterates through the available pages in the frames to locate the page to be marked as dirty. If the page is found then set the dirty bit of the page node as 1.

### forcePage
It writes the current content of the page back to the page file on disk.

* * *

## Statistics Functions
The purpose of the statistics functions was to provide information regarding the buffer pool and its contents.
The print debug functions use the statistics functions to provide information about the pool.

### getFrameContents
The function returns an array of PageNumbers (of size numPages) where the ith element is the number of the page stored in the ith page frame. An empty page frame is represented using the constant NO_PAGE.

### getDirtyFlags
The function returns an array of bools (of size numPages) where the ith element is TRUE if the page stored in the ith page frame is dirty. Empty page frames are considered as clean.

### getFixCounts
The function returns an array of ints (of size numPages) where the ith element is the fix count of the page stored in the ith page frame. Return 0 for empty page frames.

### getNumReadIO
The function returns the number of pages that have been read from disk since a buffer pool has been initialized.

### getNumWriteIO
The function returns the number of pages written to the page file since the buffer pool has been initialized.

* * *

## The Page Replacement Strategies
### pinPage_FIFO
This function implements the FIFO page replacement strategy. It checks to see if the page is in memory. If the page is found it calls the pageInMemory function described in "Helper Functions" later, and returns RC_OK. If the page is required to be loaded in the memory then first free frame is located starting from head. If an empty frame is found then the page is loaded in the found frame and page details are set. 
   Also, the found frame is updated to be the head of the linked list.
For new page, if all the frames are filled, then the function starts iterating from trail of the list to 
   locate the oldest frame with fix count 0. The found frame is updated to be the head of the linked list.
If the frame is found following above strategy then updateNewFrame function described in "Helper Functions" later; 
   otherwise, the function returns no more space in buffer error.

### pinPage_LRU
This function implements the LRU replacement policy as described in the lecture. It checks to see if the page is in memory. If it is, it calls the pageInMemory function described in
   Helper Functions later, and returns immediately with RC_OK.
Every time a frame is referenced, it is moved to the head of the framelist. So, at any moment the head will be the
   latest used frame, and the tail will be the least used frame.
If the page is not in memory, it starts iterating from the tail of the list to look for a frame with fixcount 0.
If any such frame is found, it calls the updateNewFrame function described in Helper Functions later; otherwise it
   returns no more space in buffer error.

* * *

## Additional Page Replacement Strategies
### pinPage_LRU_K
This function implements the LRU_K replacement policy as explained in the paper. It checks to see if the page is in memory. If it is, it calls the pageInMemory function described in
   Helper Functions later, and returns immediately with RC_OK.
Every time a frame is referenced, the reference number(current count of pinning) is updated in the history array
   (bminfo->khist).
If the page is not in memory, it starts iterating from the head of the list and calculate the distance as the
   difference of current count of pinning and kth reference of the page, for all pages in memory having fixcount 0.
The page with the max distance is replaced. If no page is called k time (kth reference is -1 for all pages), then
   it works just as the LRU, and checks for the least recently used page.
In any case, if any such frame is found, it calls the updateNewFrame function described in Helper Functions later; 
   otherwise it returns no more space in buffer error.

* * *

# No memory leaks
The code is tested with valgrind for no memory leaks.