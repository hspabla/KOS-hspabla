/******************************************************************************
    Copyright © 2012-2015 Martin Karsten

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/
#ifndef _Bitmap_h_
#define _Bitmap_h_ 1

#include "generic/bitmanip.h"

template<size_t X> constexpr mword BitmapEmptyHelper(const mword* bits) {
  return bits[X] | BitmapEmptyHelper<X-1>(bits);
}

template<> constexpr mword BitmapEmptyHelper<0>(const mword* bits) {
  return bits[0];
}

template<size_t X> constexpr mword BitmapFullHelper(const mword* bits) {
  return bits[X] & BitmapFullHelper<X-1>(bits);
}

template<> constexpr mword BitmapFullHelper<0>(const mword* bits) {
  return bits[0];
}

template<size_t X> constexpr mword BitmapCountHelper(const mword* bits) {
  return popcount(bits[X]) + BitmapCountHelper<X-1>(bits);
}

template<> constexpr mword BitmapCountHelper<0>(const mword* bits) {
  return popcount(bits[0]);
}

template<size_t B = bitsize<mword>()>
class Bitmap {
  static const int N = divup(B,bitsize<mword>());
  mword bits[N];
public:
  explicit Bitmap() {}
  explicit Bitmap( mword b ) { for (size_t i = 0; i < N; i += 1) bits[i] = b; }
  static constexpr bool valid( mword idx ) { return idx < N * bitsize<mword>(); }
  static constexpr Bitmap filled() { return Bitmap(~mword(0)); }
  void setB() { for (size_t i = 0; i < N; i++) bits[i] = ~mword(0); }
  void clrB() { for (size_t i = 0; i < N; i++) bits[i] =  mword(0); }
  void flpB() { for (size_t i = 0; i < N; i++) bits[i] = ~bits[i]; }
  template<bool atomic=false> void set  ( mword idx ) {
    bit_set<atomic>(bits[idx / bitsize<mword>()], idx % bitsize<mword>());
  }
  template<bool atomic=false> void clr( mword idx ) {
    bit_clr<atomic>(bits[idx / bitsize<mword>()], idx % bitsize<mword>());
  }
  template<bool atomic=false> void flp ( mword idx ) {
    bit_flp<atomic>(bits[idx / bitsize<mword>()], idx % bitsize<mword>());
  }
  constexpr bool test( mword idx ) const {
    return bits[idx / bitsize<mword>()] & (mword(1) << (idx % bitsize<mword>()));
  }
  constexpr bool empty()  const { return BitmapEmptyHelper<N-1>(bits) == mword(0); }
  constexpr bool full()   const { return  BitmapFullHelper<N-1>(bits) == ~mword(0); }
  constexpr mword count() const { return BitmapCountHelper<N-1>(bits); }
  constexpr mword find(bool findset = true, mword idx = 0) const { 
    return multiscan<N>(findset, bits, idx);
  }
  constexpr mword find_rev(bool findset = true) const {
    return multiscan_rev<N>(findset, bits);
  }
};

// B: number of bits in elementary bitmap (set to cacheline size)
// W: log2 of maximum width of bitmap
template<size_t B, size_t W>
class HierarchicalBitmap {
  static const size_t logB = floorlog2(B);
  static const size_t Levels = divup(W,logB);

  static_assert( B == pow2<size_t>(logB), "template parameter B not a power of 2" );
  static_assert( B >= bitsize<mword>(), "template parameter B less than word size" );
  static_assert( W >= logB, "template parameter W smaller than log B" );

  Bitmap<B>* bitmaps[Levels];

  size_t find(bool findset, size_t sidx, size_t ridx, size_t l) const {
    if (l == Levels) return limit<size_t>();
    size_t fidx = bitmaps[l][sidx].find(findset, ridx);
    if slowpath(fidx == B) return find(findset, ((sidx+1) / B), (sidx+1) % B, l+1);
    else if slowpath(l == 0) return sidx * B + fidx;
    else return find(findset, sidx * B + fidx, 0, l-1);
  }

public:
  static size_t allocsize( size_t bitcount ) {
    size_t mapcount = 0;
    for (size_t l = 0; l < Levels; l += 1) {
      bitcount = divup(bitcount,B);
      mapcount += bitcount;
    }
    return sizeof(Bitmap<B>) * mapcount;
  }

  void init( size_t bitcount, bufptr_t p ) {
    for (size_t l = 0; l < Levels; l += 1) {
      bitcount = divup(bitcount,B);
      bitmaps[l] = new (p) Bitmap<B>[bitcount];
      p += sizeof(Bitmap<B>) * bitcount;
    }
  }

  void set( size_t idx, size_t botlevel = 0 ) {
    for (size_t l = botlevel; l < Levels; l += 1) {
      size_t r = idx % B;
      idx = idx / B;
      if slowpath(bitmaps[l][idx].test(r)) return;
      bitmaps[l][idx].set(r);
    }
    KASSERT1(idx == 0, idx);
  }

  void clr( size_t idx, size_t botlevel = 0 ) {
    for (size_t l = botlevel; l < Levels; l += 1) {
      size_t r = idx % B;
      idx = idx / B;
      bitmaps[l][idx].clr(r);
      if slowpath(!bitmaps[l][idx].empty()) return;
    }
    KASSERT1(idx == 0, idx);
  }

  void blockset( size_t idx ) {
    bitmaps[0][idx / B].setB();
    set(idx / B, 1);
  }

  void blockclr( size_t idx ) {
    bitmaps[0][idx / B].clrB();
    clr(idx / B, 1);
  }

  constexpr bool test( size_t idx ) const {
    return bitmaps[0][idx / B].test(idx % B);
  }

  constexpr bool empty() const {
    return bitmaps[Levels-1][0].empty();
  }

  constexpr bool blockempty( size_t idx ) const {
    return bitmaps[0][idx / B].empty();
  }

  constexpr bool blockfull( size_t idx ) const {
    return bitmaps[0][idx / B].full();
  }

  size_t find(bool findset = true) const {
    return find(findset, 0, 0, Levels - 1);
  }

  size_t find(size_t idx, bool findset = true) const {
    return find(findset, (idx+1) / B, (idx+1) % B, 0);
  }

  size_t find_rev(bool findset = true) const {
    size_t idx = 0;
    for (size_t i = Levels - 1;; i -= 1) {
      size_t ldx = bitmaps[i][idx].find_rev(findset);
      if slowpath(ldx == B) return limit<size_t>();
      idx = idx * B + ldx;
      if slowpath(i == 0) return idx;
    }
  }

  size_t getrange( size_t idx, size_t bitcount ) const { // used for printing
    bool isset = test(idx);
    do {
      idx += 1;
    } while (idx < bitcount && test(idx) == isset);
    return idx;
  }
};

#endif /* _Bitmap_h_ */
