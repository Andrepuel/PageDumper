Page Dumper
===========
Set of codes to deal with PostgreSQL page files. From the PostgreSQL documentation:

> Every table and index is stored as an array of pages of a fixed size
> 
> -- 53.5. Database Page Layout

The code of this project opens, parse and extract the data from a page file. Since a page does not contain information about what is the type of it's data (only the number of attributes) the user will have to provide the table layout.

See `table_sample_user.cpp` to get started.

Features
--------
Page Dumper can iterate over pages and over ItemId. The ItemId contains a pointer to the tuple's data that also may be fetched. The page does not carry any information about the data, so the user must provide the tuple layout (see `table_sample_user.cpp:user_code()`).

The project offers functions to extract _integers_, and _varlena_ (variable length attribute) from the tuple data. It also can deal with out of table stored values (TOAST) and compressed data.

Note: Actually, compressed data that is not in a TOAST table is not supported because I did not find an instance that had this type of storage.

I had successfully used this code to retrieve data from a corrupted database.

PostgreSQL version supported
----------------------------
The project was coded and tested using PostgreSQL version 8.4. Supporting others versions is a future work.
