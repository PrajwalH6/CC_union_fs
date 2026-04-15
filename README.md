# Mini Union File System using FUSE

## Project Overview

This project implements a **Mini Union File System** using **FUSE (Filesystem in Userspace)** in C.

A union file system combines two separate directory layers into one virtual mounted filesystem:

* **Lower Layer** → acts as the base read-only directory
* **Upper Layer** → stores modified and newly written files

The mounted filesystem presents a merged view of both layers to the user.

The project follows the **copy-on-write mechanism**, where files from the lower layer are copied to the upper layer before modification.

It also uses a **whiteout mechanism** to logically delete files from the lower layer without modifying original data.

---

## My Contribution

This module implements the core filesystem operations required for basic union filesystem functionality.

### Implemented Features

* File attribute retrieval (`getattr`)
* Directory listing (`readdir`)
* File read operation (`read`)
* File write operation (`write`)
* File deletion (`unlink`)

---

## Working Principle

### Path Resolution

For every file request:

1. Check upper layer first
2. If not found, check lower layer
3. If whiteout file exists, treat file as deleted

---

### Copy-on-Write Mechanism

When a file exists only in the lower layer and a write operation is requested:

* file is copied from lower layer to upper layer
* modification is performed in upper layer

---

### Whiteout Deletion

When deleting a file that exists only in the lower layer:

* original lower file remains unchanged
* a hidden whiteout file is created in upper layer

Example:

```bash id="dr4eq7"
.wh.filename
```

This hides the lower-layer file from the mounted filesystem.

---

## Internal Functions

### build_path()

Builds complete path using base directory and relative file path.

### resolve_path()

Resolves whether the file exists in upper or lower layer.

### copy_to_upper()

Copies lower-layer file into upper layer before write.

### get_whiteout_path()

Generates whiteout file path for deletion handling.

---

## FUSE Operations Implemented

* `unionfs_getattr`
* `unionfs_readdir`
* `unionfs_read`
* `unionfs_write`
* `unionfs_unlink`

---

## Compilation

```bash id="njlwm8"
make
```

---

## Execution

Create directories:

```bash id="tyrj9n"
mkdir lower upper mount
```

Add sample file:

```bash id="5yhygh"
echo "hello world" > lower/file1.txt
```

Run filesystem:

```bash id="x39i5q"
./mini_unionfs lower upper mount
```

---

## Access Mounted Filesystem

```bash id="e9fzhf"
ls mount
cat mount/file1.txt
```

---

## Unmount Filesystem

```bash id="a1y6x4"
fusermount3 -u mount
```

---

## Technologies Used

* C Programming
* FUSE3
* Linux File System APIs
