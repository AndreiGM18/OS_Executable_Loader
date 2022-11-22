**Name: Mitran Andrei-Gabriel**
**Group: 323CA**

## Executable Loader (Homework #1)

### Description:

* This project aims to create an executable loader.
* Everything is provided by the OS team, with the exception of the signal
handler. As such, this README is going to mainly talk about said handler.
* After getting the page fault's address, the handler loops through the
segments searching for it.
* It first checks if the data void* has been allocated. It is used to tell if
a page has been mapped or not in each segment. It is allocated enough memory
for the amount of pages that would fit in the segment's mem_size + 1.
* It then gets the number of pages to jump over in order to get to the fault.
* If the page has already been mapped, the original handler is used,
as the permissions are invalid.
* If the page has not been mapped, it maps it and marks it as having been
mapped.
* If the address is before file_size, then it copies either an entire page,
or less than that if file_size is not a multiple of a page's size, from the
source file.
* Finally it sets the permissions for the newly mapped page.
* If the fault's address was not found in any segment, the original
handler is used.

### Comments:
* I decided to use the DIE macro, as I have seen it used numerous times in the
lab, for defensive programming (I have also used it during my first year).
* This homework helped me to better understand virtual memory allocation.
* Further improvements can be made, perhaps by actually providing mmap with
the fd and offset from the start or by allocating memory for the data void *
of each segment before searching for the page fault (maybe in the so_execute
function).

### Resources:
* Everything provided by the OS team
* (https://www.man7.org/linux/man-pages/index.html)

