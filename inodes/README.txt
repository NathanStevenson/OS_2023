1. Identify the perpetrator and explain why we suspect them

    - File 1: The PDF is a 20 slide slideshow on device drivers. The slides belong to Kevin Skadron in UVA CS and were created in 2004
    - File 2: The GIF seems to be some sort of heat map image with lcache, Dcache, IntReg, and many FPs. Potentially heat map image of hardware in computer (IntReg is hottest)
    - File 3: The TIFF is the image of the "ketchup dragon". We can confirm that whoever file system this is may have been the person to steal the figure. Image matches one supplied in assignment.
    - File 4: The PS once converted to a PDF was a new heat map image similar to file 2. Except this time the image displayed appears to be significantly warmer than before.
    From the File 4 raw data we can observe in the file that it was created by John Bradley a likely suspect now in this crime.

    While there is not a ton of evidence we found on the image drive, there was a photo of the ketchup dragon, a slideshow, and two heat map images. One of the heat map images
    identifies its creator as "John Bradley" so this is our most likely perpetrator. The other slideshow was created by Skadron, but it is likely this was a downloaded copy.

2. List any valid files by file type and disck block # where file's inode resides. 

    - In order to determine whether a file was valid or not we first stepped through all
    of the valid blocks inside of the image drive and tried to pattern match given the information
    we were given (UID and GID). Given that this would match all valid inodes this was a good place to 
    start; however we realized it may also by coincidence match non-inodes. In order to filter out these 
    false positive we checked if each data block the inode pointed to was between 0 and MAX_BLOCK of the image
    file. While there still is potential some inodes make it through it is extremely unlikely that the garbage inode
    would not contain some invalid data.

    - In our case we ended up finding 4 valid files and catching 16 invalid files that did not match the UID and GID pattern.

    file1: type - PDF  | disc block where inode resides - 231 | file size - 1209645
    file2: type - GIF  | disc block where inode resides - 231 | file size - 12784
    file3: type - TIFF | disc block where inode resides - 231 | file size - 7306130
    file4: type - PS   | disc block where inode resides - 231 | file size - 159322