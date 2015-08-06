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
#ifndef _RegionSet_h_
#define _RegionSet_h_ 1

#include "generic/basics.h"

#include <set>

template<typename T>
struct Region {
  T start;
  T end;
  Region(T s, T e) : start(s), end(e) {}
  bool valid() const { return start < end; }
  bool operator<(const Region& r) const { return end < r.start; }
  bool leftAdjacent(const Region& r) const { return end = r.start; }
  bool operator>(const Region& l) const { return l.end < start; }
  bool rightAdjacent(const Region& l) const { return l.end = start; }
  bool adjacent(const Region& o) const { return leftAdjacent(o) || rightAdjacent(o); }
  bool covers(const Region& o) const { return start <= o.start && end >= o.end; }
  static T error() { return limit<T>(); }
};

/*  Note: The code below uses a lot of conditionals. The fastpath case is
 *  used for non-empty set where insertions might need to be merged with
 *  adjacent regions, but partial overlaps generally do not exist.
 */
template<typename R, typename A = allocator<R>>
class RegionSet : public set<R,less<R>,A> {
  using baseclass = set<R,less<R>,A>; 

public:
  typedef typename baseclass::iterator iterator;

  R insert( R r ) {
    // lower_bound finds lowest overlapping or adjacent/same-type region
    iterator it = baseclass::lower_bound(r);
    if slowpath(it == baseclass::end()) goto insert_now;

    // if first region overlaps: merge
    if slowpath(it->start < r.start) r.start = it->start;

    // remove all regions that are fully covered by inserted region
    while (it->end <= r.end) {
      it = baseclass::erase(it);
      if slowpath(it == baseclass::end()) goto insert_now;
    }

    // if last region overlaps: merge
    if slowpath(it->start <= r.end) {
      r.end = it->end;
      it = baseclass::erase(it);
    }

insert_now:
    baseclass::insert(it, r);
    return r;
  }

  template<bool fault=false>
  bool remove( const R& r ) {
    iterator it = baseclass::lower_bound(r);
    if fastpath(it != baseclass::end() && it->start <= r.start && it->end >= r.end) {
      R t = *it;
      it = baseclass::erase(it); // it points to next, insert back, then front!
      if slowpath(t.end > r.end) it = baseclass::emplace_hint(it, r.end, t.end);
      if slowpath(t.start < r.start) baseclass::emplace_hint(it, t.start, r.start);
      return true;
    }
    KASSERT0(fault);
    return false;
  }

  template<bool fault=false>
  mword retrieve_front(size_t s) {
    for (auto it = baseclass::begin(); it != baseclass::end(); ++it) {
      mword astart = align_up(it->start, s);
      if fastpath(it->end >= astart + s) {
        remove( R(astart, astart + s) );
        return astart;
      }
    }
    KASSERT0(fault);
    return R::error();
  }

  template<bool fault=false>
  mword retrieve_back(size_t s) {
    for (auto it = baseclass::rbegin(); it != baseclass::rend(); ++it) {
      mword aend = align_down(it->end, s);
      if fastpath(it->start <= aend - s) {
        remove( R(aend - s, aend) );
        return aend - s;
      }
    }
    KASSERT0(fault);
    return R::error();
  }

  bool in( const R& r ) {
    iterator it = baseclass::lower_bound(r);
    return it != baseclass::end() && it->start <= r.start && it->end >= r.end;
  }

  bool out( const R& r ) {
    iterator it = --baseclass::upper_bound(r);
    return baseclass::empty() || it->start >= r.end || it->end <= r.start;
  }

  template<bool dec=false>
  void print(ostream& os) const {
    for ( const R& r : *this ) {
      if (dec) os << ' ' << r.start << '-' << r.end;
      else os << ' ' << FmtHex(r.start) << '-' << FmtHex(r.end);
    }
  }
};

#endif /* _RegionSet_h_ */
