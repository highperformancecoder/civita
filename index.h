/*
  @copyright Russell Standish 2020
  @author Russell Standish
  This file is part of Civita.

  Civita is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Civita is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Civita.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CIVITA_INDEX_H
#define CIVITA_INDEX_H
#include <vector>
#include <set>
#include <map>
#include <cstdint>
#include <cstdlib>
#include <assert.h>

#ifndef CLASSDESC_ACCESS
#define CLASSDESC_ACCESS(x)
#endif

// This macro allows the allocator to be swapped for a custom
// allocator, such as needed when running this under an electron
// project. See https://sourceforge.net/p/minsky/ravel/564/
#ifndef CIVITA_ALLOCATOR
#define CIVITA_ALLOCATOR std::allocator
#endif

namespace civita
{
  void trackAllocation(std::ptrdiff_t n);
    
  template <class T>
  struct LibCAllocator
  {
    using value_type=T;
    T* allocate(size_t n) {
      trackAllocation(sizeof(T)*n);
      if (auto r=reinterpret_cast<T*>(std::malloc(sizeof(T)*n))) return r;
      throw std::bad_alloc();
    }
    void deallocate(T* p, size_t n) {trackAllocation(-sizeof(T)*n); std::free(p);}
    bool operator==(const LibCAllocator&) const {return true;} // no state!
    bool operator!=(const LibCAllocator& x) const {return !operator==(x);}
  };
  
  /// represents index concept for sparse tensors
  class Index
    {
    public:
      CLASSDESC_ACCESS(Index);
      using Impl=std::vector<std::size_t,CIVITA_ALLOCATOR<std::size_t>>;
      Index() {}
      template <class T> explicit
      Index(const T& indices) {*this=indices;}
      Index(const Index&)=default;
      Index(Index&&)=default;
      Index& operator=(const Index&)=default;
      Index& operator=(Index&&)=default;

      // can only assign ordered containers
      template <class T, class C, class A>
      Index& operator=(const std::set<T,C,A>& indices) {
        index.clear(); index.reserve(indices.size());
        for (auto& i: indices) index.push_back(i);
        return *this;
      }
      template <class K, class V, class C, class A>
      Index& operator=(const std::map<K,V,C,A>& indices) {
        index.clear(); index.reserve(indices.size());
        for (auto& i: indices) index.push_back(i.first);
        return *this;
      }

#if defined(__cplusplus) && __cplusplus >= 202002L && !defined(__APPLE__)
      bool operator<=>(const Index&) const=default;
#endif
      
      /// return hypercube index corresponding to lineal index i 
      std::size_t operator[](std::size_t i) const {return index.empty()? i: index[i];}
      // invariant, should always be true
      bool sorted() const {
        std::set<std::size_t,std::less<std::size_t>,CIVITA_ALLOCATOR<std::size_t>>
          tmp(index.begin(), index.end());
        return tmp.size()==index.size();
      }
      bool empty() const {return index.empty();}
      std::size_t size() const {return index.size();}
      void clear() {index.clear();}
      /// return the lineal index of hypercube index h, or size if not present 
      std::size_t linealOffset(std::size_t h) const;
      Index::Impl::const_iterator begin() const {return index.begin();}
      Index::Impl::const_iterator end() const {return index.end();}
    private:
      Impl index; // sorted index vector
      // For optimisation to avoid map<=>vector transformation
      friend class PermuteAxis;
      friend class Pivot;
      friend class ReductionOp;
      friend class Slice;
      // optimised transfer routines - private because can't guarantee index uniqueness
      void assignVector(Impl&& indices) {index=std::move(indices); assert(sorted());}
      template <class T, class A>
      void assignVector(const std::vector<T,A>& indices) {
        index.clear(); index.reserve(indices.size());
        for (auto& i: indices) index.push_back(i);
         assert(sorted());
      }
      template <class F, class S, class A>
      void assignVector(const std::vector<std::pair<F,S>,A>& indices) {
        index.clear(); index.reserve(indices.size());
        for (auto& i: indices) index.push_back(i.first);
         assert(sorted());
      }
    };
    

}
#endif
