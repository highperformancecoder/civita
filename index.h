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
#include <cstddef>

#ifndef CLASSDESC_ACCESS
#define CLASSDESC_ACCESS(x)
#endif

namespace civita
{
  /// represents index concept for sparse tensors
  class Index
    {
      std::vector<std::size_t> index; // sorted index vector
      CLASSDESC_ACCESS(Index);
    public:
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
      bool sorted() const
      {std::set<std::size_t> tmp(index.begin(), index.end()); return tmp.size()==index.size();}
      bool empty() const {return index.empty();}
      std::size_t size() const {return index.size();}
      void clear() {index.clear();}
      /// return the lineal index of hypercube index h, or size if not present 
      std::size_t linealOffset(std::size_t h) const;
      std::vector<std::size_t>::const_iterator begin() const {return index.begin();}
      std::vector<std::size_t>::const_iterator end() const {return index.end();}
    };
    

}
#endif
