# Mini Union File System using FUSE

## Overview

This project implements a **Mini Union File System (MiniUnionFS)** using **FUSE (Filesystem in Userspace)** in C.

A union file system combines two directories:

* **Lower directory** → read-only base layer
* **Upper directory** → writable layer

The mounted filesystem presents a merged view of both directories.

This implementation supports:

* File attribute retrieval
* Directory listing
* File reading
* File writing
* File deletion using whiteout mechanism

---

## Concept

The filesystem works using **copy-on-write**:

* If a file exists only in the lower layer and is modified,
* it is first copied to the upper layer,
* then modifications are applied.

Deletion of lower-layer files is handled using **whiteout files**:

* A hidden file named `.wh.filename` is created in the upper layer
* This hides the corresponding file from the lower layer

---

## Features Implemented

### 1. getattr

Retrieves file metadata using `lstat()`.

### 2. readdir

Reads directory contents from:

* lower layer
* upper layer

Whiteout files are ignored during listing.

### 3. read

Reads file data from:

* upper layer if present
* otherwise lower layer

### 4. write

Implements copy-on-write:

* copies lower file to upper layer if needed
* writes only to upper layer

### 5. unlink

Deletes files:

* directly removes from upper layer if present
* creates whiteout file if file exists only in lower layer

---

## Project Structure

```bash
mini_unionfs.c
README.md
lower/
upper/
mount/
```

---

## Compilation

Compile using:

```bash
gcc mini_unionfs.c -o mini_unionfs `pkg-config fuse3 --cflags --libs`
```

---

## Running the Filesystem

Create required directories:

```bash
mkdir lower upper mount
```

Add sample file:

```bash
echo "hello world" > lower/file1.txt
```

Run:

```bash
./mini_unionfs lower upper mount
```

---

## Access Mounted Filesystem

View files:

```bash
ls mount
```

Read file:

```bash
cat mount/file1.txt
```

---

## Write Operation Example

Modify file:

```bash
echo "new data" >> mount/file1.txt
```

This triggers:

* copy from lower → upper
* write in upper layer

---

## Delete Operation Example

Delete file:

```bash
rm mount/file1.txt
```

If file exists only in lower layer:

whiteout file created:

```bash
upper/.wh.file1.txt
```

---

## Unmount Filesystem

```bash
fusermount3 -u mount
```

---

## Internal Functions

### build_path()

Constructs full path from base + relative path.

### resolve_path()

Resolves whether file exists in:

* upper layer
* lower layer

### copy_to_upper()

Copies lower file into upper layer during write.

### get_whiteout_path()

Creates whiteout filename.

---

## Technologies Used

* C Programming
* FUSE3
* Linux File System APIs

---

## Learning Outcomes

This project demonstrates:

* Filesystem layering
* Copy-on-write mechanism
* Whiteout deletion handling
* FUSE callback operations

---

## Author

Implemented as part of Operating Systems / File Systems learning project.


