
utf8_valid.h
============

This header file provides functions to validate UTF-8 encoding form according to the specification published by Unicode and ISO/IEC 10646:2011.


```c

bool    utf8_valid(const char *src, size_t len);
bool    utf8_check(const char *src, size_t len, size_t *cursor);
size_t  utf8_maximal_subpart(const char *src, size_t len);

```
