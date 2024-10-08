/*
  @copyright Steve Keen 2019
  @author Russell Standish
  This file is part of Minsky.

  Minsky is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Minsky is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Minsky.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "hypercube.h"
#include <set>

#ifdef CLASSDESC
#include "dimension.jcd"
#include "xvector.jcd"
#include "hypercube.jcd"
#include <classdesc_epilogue.h>
#endif

using namespace std;

namespace civita
{
  std::vector<unsigned> Hypercube::dims() const
  {
    std::vector<unsigned> d;
    for (auto& i: xvectors) d.push_back(i.size());
    return d;
  }
  
  std::vector<string> Hypercube::dimLabels() const
  {
    std::vector<std::string> l;			  
    for (auto& i: xvectors) l.push_back(static_cast<std::string>(i.name));
    return l;
  }      

  std::vector<unsigned> const& Hypercube::dims(const std::vector<unsigned>& d) {
    xvectors.clear();
    for (size_t i=0; i<d.size(); ++i)
      {
        xvectors.emplace_back(std::to_string(i));
        xvectors.back().dimension.type=Dimension::value;
        for (size_t j=0; j<d[i]; ++j)
          xvectors.back().emplace_back(double(j));
      }
    return d;
  }

  size_t Hypercube::numElements() const
    {
      size_t s=1;
      for (auto& i: xvectors)
        s*=i.size();
      return s;
    }

  double Hypercube::logNumElements() const
  {
    double r=0;
    for (auto& i: xvectors)
      r+=log(i.size());
    return r;
  }

  bool Hypercube::dimsAreDistinct() const
  {
    set<string> names;
    for (auto& xv: xvectors)
      if (!names.insert(xv.name).second)
        return false;
    return true;
  }

  
  /// split lineal index into components along each dimension
  vector<size_t> Hypercube::splitIndex(size_t i) const
  {
    std::vector<size_t> splitIndex;
    splitIndex.reserve(xvectors.size());
    for (auto& xv: xvectors)
      {
        auto res=div(ssize_t(i),ssize_t(xv.size()));
        splitIndex.push_back(res.rem);
        i=res.quot;
      }
    return splitIndex;
  }

  template 
  size_t Hypercube::linealIndex<vector<size_t>>(const vector<size_t>& splitIndex) const;

  void unionHypercube(Hypercube& result, const Hypercube& x, bool intersection)
  {
    if (x.logNumElements()==1)
      {
        result.xvectors.clear(); // intersection empty anyway
        return;
      }
    map<string, set<any, AnyLess>> indexedData;
    vector<XVector> extraDims;
    for (auto& xvector: result.xvectors)
      {
        auto& xvectorData=indexedData[xvector.name];
        for (auto& i: xvector)
          xvectorData.insert(i);
      }
    for (auto& xvector: x.xvectors)
      {
        auto xvectorData=indexedData.find(xvector.name);
        if (xvectorData==indexedData.end())
          extraDims.emplace_back(xvector);
        else
          {
            if (xvector.dimension.type==Dimension::string)
              {
                // compute the intersection of the two x-vectors.
                set<any, AnyLess> xvectorAsSet(xvector.begin(), xvector.end());
                for (auto i=xvectorData->second.begin(); i!=xvectorData->second.end(); )
                  if (xvectorAsSet.count(*i))
                    ++i;
                  else
                    i=xvectorData->second.erase(i);
              }
            else if (intersection)
              {
                if (xvectorData->second.empty())
                  {
                    result.xvectors.clear(); // intersection empty anyway
                    return;
                  }
                // trim to intersection of the two
                // TODO use structured binding when we go C++17.
                auto xRange=std::minmax_element(xvector.begin(),xvector.end());
                auto minX=*xRange.first, maxX=*xRange.second;
                auto rRange=std::minmax_element(xvectorData->second.begin(),xvectorData->second.end());
                if (minX<*rRange.first) minX=*rRange.first;
                if (*rRange.second<maxX) maxX=*rRange.second;
                for (auto i=xvectorData->second.begin(); i!=xvectorData->second.end(); )
                  {
                    auto j=i++;
                    if (*j<minX || maxX<*j)
                      xvectorData->second.erase(j);
                  }
                for (auto& i: xvector)
                  if (diff(minX,i)<=0 && diff(i,maxX)<=0)
                    xvectorData->second.insert(i);
              }
            else
              xvectorData->second.insert(xvector.begin(), xvector.end());
          }
      }
    for (auto& xvector: result.xvectors)
      {
        auto& xvectorData=indexedData[xvector.name];
        xvector.clear();
        xvector.insert(xvector.end(), xvectorData.begin(), xvectorData.end());
      }
    for (auto& i: extraDims)
      result.xvectors.push_back(i);
  }

  string Hypercube::json() const
  {
#ifdef CLASSDESC
    return classdesc::json(*this);
#else
    return "";
#endif
  }

  Hypercube Hypercube::fromJson(const std::string& s)
  {
    Hypercube hc;
#ifdef CLASSDESC
    classdesc::json(hc,s);
#endif
    return hc;
  }
}

