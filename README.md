# Position-independent data in C++

***This is very much work in progress. It works fine for an internal use case of mine, but has no documentation yet and
a lot
of rough edges. Use at your own risk!***

This project aims to provide to data
what [position-independent code](https://en.wikipedia.org/wiki/Position-independent_code) is for code.

The goal is to enable complex data structures, which are stored in a blob of memory (this could be a memory-mapped file)
and enable very efficient read-only access.

This is achieved by using relative offsets for all references between objects.

Similar techniques are used by projects like

* [FlatBuffers](https://flatbuffers.dev/)
* [Cap'n Proto](https://capnproto.org/)

In contrast to these, the present project

* only works with C++ and does not require a schema language
* does not have any limitations concerning the blob size (FlatBuffers supports only 2 GiB) and the size of arrays (
  except for the limitations which are caused by the target architecture and available memory and disk space)
