The Buffer Manager

## Introduction

In this stage, you will be implementing a buffer manager for the database system, termed "Minirel". The database buffer pool is an integral part of modern database systems. It involves managing an array of fixed-sized memory buffers called frames which hold the database pages or disk blocks.

- A **page** is the unit of transfer between the disk and the buffer pool in the main memory.
- Most modern database systems use a page size of at least 8,192 bytes.
- A database page in memory mirrors the corresponding page on disk when first read.
- A page in memory can be updated, making it different from the one on disk. Such pages are called “dirty”.

The buffer manager's main job is to decide which pages are memory resident at any given time. The mechanism and policies to decide which page to replace when a free frame is needed are crucial.

## Buffer Replacement Policies and the Clock Algorithm

Many policies like FIFO, MRU, and LRU exist for buffer replacement. However, LRU can be expensive. Hence, many systems use the clock algorithm, an approximation to LRU that's faster.

The clock algorithm involves a circular list of frames with an associated reference bit (refbit). The algorithm uses a clock hand that advances and checks the refbit of frames. The algorithm's execution details are illustrated in the provided figure.



## Structure of the Buffer Manager

Minirel's buffer manager uses three classes:

1. **BufMgr** - The main buffer manager class.
2. **BufDesc** - Describes the state of each frame.
3. **BufHashTbl** - Maps file and page numbers to buffer pool frames.

For a detailed breakdown and methods of each class, refer to the main document.

## Getting Started

Start by downloading the project files:

- `makefile`
- `buf.h`, `buf.C`
- `bufHash.C`
- `db.h`, `db.C`
- `page.C`, `page.h`
- `error.h`, `error.C`
- `testbuf.C`

Run `make` in the project directory to build the project.

## Coding and Testing

Adhere to object-oriented programming principles. Your code should be clean, well-documented, and have clearly defined classes. Ensure every file has the team members' names and descriptions. Each function should be documented for its purpose and input-output parameters.

## Handing In

Follow your instructor's guidance for submission.

## Error Handling

Use the provided `Error` class for error handling. Ensure to check function return codes and print messages using the `Error` class.
