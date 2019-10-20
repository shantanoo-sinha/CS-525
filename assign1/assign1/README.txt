The goal of this assignment is to implement a simple storage manager - a module that is capable of reading blocks from a file on disk into memory and writing blocks from memory to a file on disk. 
The storage manager deals with pages (blocks) of fixed size (PAGE_SIZE). In addition to reading and writing pages from a file, it provides methods for creating, opening, and closing files. 

The storage manager maintains several types of information for an open file: 
	- the number of total pages in the file
	- the current page position (for reading and writing)
	- the file name
	- and a POSIX file descriptor or FILE pointer