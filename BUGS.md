KOS - Known Bugs
================

- **address space destruction fails**

    The assertion `(!destroyPrev)` at the end of `AddressSpace& switchTo()` occasionally fails. To fix, `Process::exec()` must leave the new address space, before the first thread resumes.

### Feedback / Questions

Please send any questions or feedback to mkarsten|**at**|uwaterloo.ca.

