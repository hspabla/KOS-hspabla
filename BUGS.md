KOS - Known Bugs
================

- **memory management deadlock**

    Because the current FrameManager implementation uses dynamic memory for small (4K) frames, certain call sequences can lead to deadlock. This will be addressed by a new version FrameManager that does not rely on dynamic memory for small frames.

- **address space destruction fails**

    The assertion `(!destroyPrev)` at the end of `AddressSpace& switchTo()` occasionally fails. The reason for this is currently unknown.

### Feedback / Questions

Please send any questions or feedback to mkarsten|**at**|uwaterloo.ca.

