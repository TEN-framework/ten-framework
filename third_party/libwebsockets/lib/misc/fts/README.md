# LWS Full Text Search

## Introduction

![lwsac flow](/doc-assets/lws-fts.svg)

The general approach is to scan one or more UTF-8 input text "files" (they may
only exist in memory) and create an in-memory optimized trie for every token in
the file.

This can then be serialized out to disk in the form of a single index file (no
matter how many input files were involved or how large they were).

The implementation is designed to be modest on memory and cpu for both index
creation and querying, and suitable for weak machines with some kind of random
access storage.  For searching only memory to hold results is required, the
actual searches and autocomplete suggestions are done very rapidly by seeking
around structures in the on-disk index file.

Function|Related Link
---|---
Public API|[include/libwebsockets/lws-fts.h](https://libwebsockets.org/git/libwebsockets/tree/include/libwebsockets/lws-fts.h)
CI test app|[minimal-examples/api-tests/api-test-fts](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/api-tests/api-test-fts)
Demo minimal example|[minimal-examples/http-server/minimal-http-server-fulltext-search](https://libwebsockets.org/git/libwebsockets/tree/minimal-examples/http-server/minimal-http-server-fulltext-search)
Live Demo|[https://libwebsockets.org/ftsdemo/](https://libwebsockets.org/ftsdemo/)

## Query API overview

Searching returns a potentially very large lwsac allocated object, with contents
and max size controlled by the members of a struct lws_fts_search_params passed
to the search function.  Three kinds of result are possible:

### Autocomplete suggestions

These are useful to provide lists of extant results in
realtime as the user types characters that constrain the search.  So if the
user has typed 'len', any hits for 'len' itself are reported along with
'length', and whatever else is in the index beginning 'len'..  The results are
selected using and are accompanied by an aggregated count of results down that
path, and the results so the "most likely" results already measured by potential
hits appear first.
 
These results are in a linked-list headed by `result.autocomplete_head` and
each is in a `struct lws_fts_result_autocomplete`.
 
They're enabled in the search results by giving the flag
 `LWSFTS_F_QUERY_AUTOCOMPLETE` in the search parameter flags.
 
### Filepath results 

Simply a list of input files containing the search term with some statistics,
one file is mentioned in a `struct lws_fts_result_filepath` result struct.

This would be useful for creating a selection UI to "drill down" to individual
files when there are many with matches.

This is enabled by the `LWSFTS_F_QUERY_FILES` search flag.

### Filepath and line results
 
Same as the file path list, but for each filepath, information on the line
numbers and input file offset where the line starts are provided.

This is enabled by `LWSFTS_F_QUERY_FILE_LINES`... if you additionally give
`LWSFTS_F_QUERY_QUOTE_LINE` flag then the contents of each hit line from the
input file are also provided.
 
## Result format inside the lwsac

A `struct lws_fts_result` at the start of the lwsac contains heads for linked-
lists of autocomplete and filepath results inside the lwsac.

For autocomplete suggestions, the string itself is immediately after the
`struct lws_fts_result_autocomplete` in memory.  For filepath results, after
each `struct lws_fts_result_filepath` is

 - match information depending on the flags given to the search
 - the filepath string
 
You can always skip the line number table to get the filepath string by adding
.matches_length to the address of the byte after the struct.

The matches information is either

 - 0 bytes per match
 
 - 2x int32_t per match (8 bytes) if `LWSFTS_F_QUERY_FILE_LINES` given... the
   first is the native-endian line number of the match, the second is the
   byte offset in the original file where that line starts

 - 2 x int32_t as above plus a const char * if `LWSFTS_F_QUERY_QUOTE_LINE` is
   also given... this points to a NUL terminated string also stored in the
   results lwsac that contains up to 255 chars of the line from the original
   file.  In some cases, the original file was either virtual (you are indexing
   a git revision) or is not stored with the index, in that case you can't
   usefully use `LWSFTS_F_QUERY_QUOTE_LINE`.

To facilitate interpreting what is stored per match, the original search flags
that created the result are stored in the `struct lws_fts_result`.

## Indexing In-memory and serialized to file

When creating the trie, in-memory structs are used with various optimization
schemes trading off memory usage for speed.  While in-memory, it's possible to
add more indexed filepaths to the single index.  Once the trie is complete in
terms of having indexed everything, it is serialized to disk.

These contain many additional housekeeping pointers and trie entries which can
be optimized out.  Most in-memory values must be held literally in large types,
whereas most of the values in the serialized file use smaller VLI which use
more or less bytes according to the value.  So the peak memory requirements for
large tries are much bigger than the size of the serialized trie file that is
output.

For the linux kernel at 4.14 and default indexing list on a 2.8GHz AMD
threadripper (using one thread), the stats are:

Name|Value
---|---
Files indexed|52932
Input corpus size|694MiB
Indexing cpu time|50.1s (>1000 files / sec; 13.8MBytes/sec)
Peak alloc|78MiB
Serialization time|202ms
Trie File size|347MiB

To index libwebsockets main branch under the same conditions:

Name|Value
---|---
Files indexed|489
Input corpus size|3MiB
Indexing time|123ms
Peak alloc|3MiB
Serialization time|1ms
Trie File size|1.4MiB


Once it's generated, querying the trie file is very inexpensive, even when there
are lots of results.

 - trie entry child lists are kept sorted by the character they map to.  This
   allows discovering there is no match as soon as a character later in the
   order than the one being matched is seen
   
 - for the root trie, in addition to the linked-list child + sibling entries,
   a 256-entry pointer table is associated with the root trie, allowing one-
   step lookup.  But as the table is 2KiB, it's too expensive to use on all
   trie entries

## Structure on disk

All explicit multibyte numbers are stored in Network (MSB-first) byte order.

 - file header
 - filepath line number tables
 - filepath information
 - filepath map table
 - tries, trie instances (hits), trie child tables

### VLI coding

VLI (Variable Length Integer) coding works like this

[b7 EON] [b6 .. b0  DATA]

If EON = 0, then DATA represents the Least-significant 7 bits of the number.
if EON = 1, DATA represents More-significant 7-bits that should be shifted
left until the byte with EON = 0 is found to terminate the number.

The VLI used is predicated around 32-bit unsigned integers

Examples:

 - 0x30            =    48
 - 0x81 30         =   176
 - 0x81 0x80 0x00  = 16384

Bytes | Range
---|---
1|<= 127
2|<= 16K - 1
3|<= 2M -1
4|<= 256M - 1
5|<= 4G - 1

The coding is very efficient if there's a high probabilty the number being
stored is not large.  So it's great for line numbers for example, where most
files have less that 16K lines and the VLI for the line number fits in 2 bytes,
but if you meet a huge file, the VLI coding can also handle it.

All numbers except a few in the headers that are actually written after the
following data are stored using VLI for space- efficiency without limiting
capability.  The numbers that are fixed up after the fact have to have a fixed
size and can't use VLI.

### File header

The first byte of the file header where the magic is, is "fileoffset" 0.  All
the stored "fileoffset"s are relative to that.

The header has a fixed size of 16 bytes.

size|function
---|---
32-bits|Magic 0xCA7A5F75
32-bits|Fileoffset to root trie entry
32-bits|Size of the trie file when it was created (to detect truncation)
32-bits|Fileoffset to the filepath map
32-bits|Number of filepaths

### Filepath line tables

Immediately after the file header are the line length tables.

As the input files are parsed, line length tables are written for each file...
at that time the rest of the parser data is held in memory so nothing else is
in the file yet.  These allow you to map logical line numbers in the file to
file offsets space- and time- efficiently without having to walk through the
file contents.

The line information is cut into blocks, allowing quick skipping over the VLI
data that doesn't contain the line you want just by following the 8-byte header
part.

Once you find the block with your line, you have to iteratively add the VLIs
until you hit the one you want.

For normal text files with average line length below 128, the VLIs will
typically be a single byte.  So a block of 200 line lengths is typically
208 bytes long.

There is a final linetable chunk consisting of all zeros to indicate the end
of the filepath line chunk series for a filepath.

size|function
---|---
16-bit|length of this chunk itself in bytes
16-bit|count of lines covered in this chunk
32-bit|count of bytes in the input file this chunk covers 
VLI...|for each line in the chunk, the number of bytes in the line


### Filepaths

The single trie in the file may contain information from multiple files, for
example one trie may cover all files in a directory.  The "Filepaths" are
listed after the line tables, and referred to by index thereafter.

For each filepath, one after the other:

size|function
---|---
VLI|fileoffset of the start of this filepath's line table
VLI|count of lines in the file
VLI|length of filepath in bytes
...|the filepath (with no NUL)

### Filepath map

To facilitate rapid filepath lookup, there's a filepath map table with a 32-bit
fileoffset per filepath.  This is the way to convert filepath indexes to
information on the filepath like its name, etc

size|function
---|---
32-bit...|fileoffset to filepath table for each filepath

### Trie entries

Immediately after that, the trie entries are dumped, for each one a header:

#### Trie entry header

size|function
---|---
VLI|Fileoffset of first file table in this trie entry instance list
VLI|number of child trie entries this trie entry has
VLI|number of instances this trie entry has

The child list follows immediately after this header

#### Trie entry instance file

For each file that has instances of this symbol:

size|function
---|---
VLI|Fileoffset of next file table in this trie entry instance list
VLI|filepath index
VLI|count of line number instances following

#### Trie entry file line number table

Then for the file mentioned above, a list of all line numbers in the file with
the symbol in them, in ascending order.  As a VLI, the median size per entry
will typically be ~15.9 bits due to the probability of line numbers below 16K.

size|function
---|---
VLI|line number
...

#### Trie entry child table

For each child node

size|function
---|---
VLI|file offset of child
VLI|instance count belonging directly to this child
VLI|aggregated number of instances down all descendent paths of child
VLI|aggregated number of children down all descendent paths of child
VLI|match string length
...|the match string
