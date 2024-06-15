/*
  @copyright Russell Standish 2019
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

#include "tensorOp.h"
#include <algorithm>
#include <exception>
#include <set>
using namespace std;

#ifdef CLASSDESC
#include <classdesc_epilogue.h>
#endif

namespace civita
{
  std::atomic<bool> ITensor::s_cancel{false};
  
  void BinOp::setArguments(const TensorPtr& a1, const TensorPtr& a2, const Args&)
  {
    arg1=a1; arg2=a2;
    if (arg1 && arg1->rank()!=0)
      {
        hypercube(arg1->hypercube());
        if (arg2 && arg2->rank()!=0 && arg1->hypercube().dims()!=arg2->hypercube().dims())
          throw std::runtime_error("arguments not conformal");
        
      }
    else if (arg2)
      hypercube(arg2->hypercube());
    else
      hypercube(Hypercube());
    set<size_t> indices;
    if (arg1) indices.insert(arg1->index().begin(), arg1->index().end());
    if (arg2 && !arg2->index().empty())
      {
        set<size_t> indices2;
        indices2.insert(arg2->index().begin(), arg2->index().end());
        if (indices.empty())
          indices=std::move(indices2);
        else // find set intersection
          for (auto i=indices.begin(); i!=indices.end(); checkCancel())
            {
              auto j=i; i++;
              // clearing indices completely causes all elements of the hypercube to be evaluated uselessly
              if (!indices2.count(*j) && indices.size()>1)
                indices.erase(j);
            }
      }
    m_index=indices;
  }


  void ReduceArguments::setArguments(const vector<TensorPtr>& a,const Args&)
  {
    hypercube({});
    if (!a.empty())
      {
        auto hc=a[0]->hypercube();
        hypercube(hc);
        set<size_t> idx;
        for (const auto& i: a)
          {
            if (i->rank()>0 && hc.rank()>0 && i->hypercube()!=hc)
              throw runtime_error("arguments not conformal");
            idx.insert(i->index().begin(), i->index().end());
          }
        m_index=idx;
      }
    args=a;
  }

  double ReduceArguments::operator[](size_t i) const
  {
    if (args.empty()) return init;
    assert(i<size());
    double r=init; 
    for (const auto& j: args)
      {
        auto x=j->rank()==0? (*j)[0]: (*j)[i];
        if (!isnan(x)) f(r, x);
      }
    return r;
  }

  ITensor::Timestamp ReduceArguments::timestamp() const
  {
    Timestamp t;
    for (const auto& i: args)
      t=max(t, i->timestamp());
    return t;
  }
  
  double ReduceAllOp::operator[](size_t) const
  {
    double r=init;
    for (size_t i=0; i<arg->size(); checkCancel(), ++i)
      {
        double x=(*arg)[i];
        if (!isnan(x)) f(r,x,i);
      }
    return r;
  }

  void ReductionOp::setArgument(const TensorPtr& a,  const Args& args)
  {
    arg=a;
    dimension=std::numeric_limits<size_t>::max();
    if (arg)
      {
        const auto& ahc=arg->hypercube();
        m_hypercube=ahc;
        auto& xv=m_hypercube.xvectors;
        for (auto i=xv.begin(); i!=xv.end(); ++i)
          if (i->name==args.dimension)
            dimension=i-xv.begin();
        if (dimension<arg->rank())
          {
            xv.erase(xv.begin()+dimension);
            // compute index - enter index elements that have any in the argument
            set<size_t> indices;
            for (size_t i=0; i<arg->size(); checkCancel(), ++i)
              {
                auto splitIdx=ahc.splitIndex(arg->index()[i]);
                SOI soi{i,splitIdx[dimension]};
                splitIdx.erase(splitIdx.begin()+dimension);
                auto idx=m_hypercube.linealIndex(splitIdx);
                sumOverIndices[idx].emplace_back(soi);
                indices.insert(idx);
              }
            m_index=indices;
          }
        else
          m_hypercube.xvectors.clear(); //reduce all, return scalar
      }
    else
      m_hypercube.xvectors.clear();
  }

  
  double ReductionOp::operator[](size_t i) const
  {
    assert(i<size());
    if (!arg) return init;
    if (dimension>arg->rank())
      return ReduceAllOp::operator[](i);

    double r=init;
    if (index().empty())
      {
        auto argDims=arg->shape();
        size_t stride=1;
        for (size_t j=0; j<dimension; ++j)
          stride*=argDims[j];
        auto quotRem=ldiv(i, stride); // quotient and remainder calc in one hit
        auto start=quotRem.quot*stride*argDims[dimension] + quotRem.rem;
        assert(stride*argDims[dimension]>0);
        for (size_t j=0; j<argDims[dimension]; checkCancel(), ++j)
          {
            double x=arg->atHCIndex(j*stride+start);
            if (!isnan(x)) f(r,x,j);
          }
      }
    else
      {
        auto soi=sumOverIndices.find(index()[i]);
        assert(soi!=sumOverIndices.end());
        if (soi!=sumOverIndices.end())
          for (auto j: soi->second)
            {
              checkCancel();
              double x=(*arg)[j.index];
              if (!isnan(x)) f(r,x,j.dimIndex);
            }
      }
    return r;
  }

  const Hypercube& CachedTensorOp::hypercube() const
  {
    if (m_timestamp<timestamp()) {
      computeTensor();
      m_timestamp=Timestamp::clock::now();
    }
    return cachedResult.hypercube();
  }
  
  double CachedTensorOp::operator[](size_t i) const
  {
    assert(i<size());
    if (m_timestamp<timestamp()) {
      computeTensor();
      m_timestamp=Timestamp::clock::now();
    }
    return cachedResult[i];
  }

  void DimensionedArgCachedOp::setArgument(const TensorPtr& a, const Args& args)
  {
    arg=a;
    argVal=args.val;
    if (!arg) {m_hypercube.xvectors.clear(); return;}
    dimension=std::numeric_limits<size_t>::max();
    auto hc=arg->hypercube();
    auto& xv=hc.xvectors;
    for (auto i=xv.begin(); i!=xv.end(); ++i)
      if (i->name==args.dimension)
        dimension=i-xv.begin();
    hypercube(std::move(hc));
  }

  
  void Scan::computeTensor() const
  {
    if (!arg) return;
    if (dimension<arg->rank())
      {
        auto argDims=arg->hypercube().dims();
        size_t stride=1;
        for (size_t j=0; j<dimension; ++j)
          stride*=argDims[j];
        if (argVal>=1 && argVal<argDims[dimension])
          // argVal is interpreted as the binning window. -ve argVal ignored
          for (size_t i=0; i<cachedResult.hypercube().numElements(); i+=stride*argDims[dimension])
            for (size_t j=0; j<stride; ++j)
              for (size_t j1=0; j1<argDims[dimension]*stride; j1+=stride)
                {
                  size_t k=i+j+max(ssize_t(0), ssize_t(j1-ssize_t(argVal-1)*stride));
                  cachedResult[i+j+j1]=arg->atHCIndex(i+j+j1);
                  for (; k<i+j+j1; checkCancel(), k+=stride)
                    f(cachedResult[i+j+j1], arg->atHCIndex(k), k);
              }
        else // scan over whole dimension
          for (size_t i=0; i<cachedResult.hypercube().numElements(); i+=stride*argDims[dimension])
            for (size_t j=0; j<stride; ++j)
              {
                cachedResult[i+j]=arg->atHCIndex(i+j);
                for (size_t k=i+j+stride; k<i+j+stride*argDims[dimension]; checkCancel(), k+=stride)
                  {
                    cachedResult[k] = cachedResult[k-stride];
                    f(cachedResult[k], arg->atHCIndex(k), k);
                  }
              }
          }
    else
      {
        cachedResult[0]=arg->atHCIndex(0);
        for (size_t i=1; i<cachedResult.hypercube().numElements(); checkCancel(), ++i)
          {
            cachedResult[i]=cachedResult[i-1];
            f(cachedResult[i], arg->atHCIndex(i), i);
          }
      }
  }

  void Slice::setArgument(const TensorPtr& a,const Args& args)
  {
    arg=a;
    sliceIndex=args.val;
    if (arg)
      {
        auto& xv=arg->hypercube().xvectors;
        Hypercube hc;
        // find axis where slicing along
        split=1;
        size_t splitAxis=0;
        auto i=xv.begin();
        for (; i!=xv.end(); ++i)
          if (i->name==args.dimension)
            {
              stride=split*i->size();
              break;
            }
          else
            {
              hc.xvectors.push_back(*i);
              split*=i->size();
              splitAxis++;
            }

        if (i==xv.end())
          split=stride=1;
        else
          for (i++; i!=xv.end(); ++i)
            // finish building hypercube
            hc.xvectors.push_back(*i);
        hypercube(hc);

        // set up index vector
        auto& ahc=arg->hypercube();
        map<size_t, size_t> ai;
        for (size_t k=0; k<arg->index().size(); checkCancel(), ++k)
          {
            auto splitIdx=ahc.splitIndex(arg->index()[k]);
            if (splitIdx[splitAxis]==sliceIndex)
              {
                splitIdx.erase(splitIdx.begin()+splitAxis);
                auto l=hc.linealIndex(splitIdx);
                ai[l]=k;
              }
          }
        m_index=ai;
        arg_index.resize(ai.size());
        // convert into lineal addressing
        size_t j=0;
        for (auto& k: ai)
          {
            checkCancel();
            arg_index[j++]=k.second;
          }
      }
  }

  double Slice::operator[](size_t i) const
  {
    assert(i<size());
    if (m_index.empty())
      {
        auto res=div(ssize_t(i), ssize_t(split));
        return arg->atHCIndex(res.quot*stride + sliceIndex*split + res.rem);
      }
    return (*arg)[arg_index[i]];
  }
  
  void Pivot::setArgument(const TensorPtr& a,const Args&)
  {
    arg=a;
    vector<string> axes;
    for (auto& i: arg->hypercube().xvectors)
      axes.push_back(i.name);
    setOrientation(axes);
  }

  
  void Pivot::setOrientation(const vector<string>& axes)
  {
    map<string,size_t> pMap;
    map<string,XVector> xVectorMap;
    auto& ahc=arg->hypercube();
    for (size_t i=0; i<ahc.xvectors.size(); checkCancel(), ++i)
      {
        pMap[ahc.xvectors[i].name]=i;
        xVectorMap[ahc.xvectors[i].name]=ahc.xvectors[i];
      }
    Hypercube hc;
    permutation.clear();
    set<string> axisSet;
    map<size_t, size_t> invPermutation;
    for (auto& i: axes)
      {
        axisSet.insert(i);
        auto v=pMap.find(i);
        if (v==pMap.end())
          throw runtime_error("axis "+i+" not found in argument");
        invPermutation[v->second]=permutation.size();
        permutation.push_back(v->second);
        hc.xvectors.push_back(xVectorMap[i]);
      }
    // add remaining axes to permutation in found order
    for (size_t i=0; i<ahc.xvectors.size(); checkCancel(), ++i)
      {
        if (!axisSet.count(ahc.xvectors[i].name))
          {
            invPermutation[i]=permutation.size();
            permutation.push_back(i);
            hc.xvectors.push_back(ahc.xvectors[i]);
          }
      }

    assert(hc.rank()==arg->rank());
    hypercube(std::move(hc));
    // permute the index vector
    map<size_t, size_t> pi;
    for (size_t i=0; i<arg->index().size(); ++i)
      {
        auto idx=arg->hypercube().splitIndex(arg->index()[i]);
        auto pidx=idx;
        for (size_t j=0; j<idx.size(); checkCancel(), ++j)
          {
            assert(invPermutation.count(j));
            assert(invPermutation[j]<pidx.size());
            pidx[invPermutation[j]]=idx[j];
          }
        auto l=hypercube().linealIndex(pidx);
        assert(pi.count(l)==0);
        pi[l]=i;
      }
    m_index=pi;
    // convert to lineal indexing
    permutedIndex.clear();
    for (auto& i: pi) permutedIndex.push_back(i.second);
    if (!permutedIndex.empty()) permutation.clear(); // not used in sparse case
  }

  size_t Pivot::pivotIndex(size_t index) const
  {
    auto idx=hypercube().splitIndex(index);
    auto pidx=idx;
    for (size_t i=0; i<idx.size(); checkCancel(), ++i)
      pidx[permutation[i]]=idx[i];
    return arg->hypercube().linealIndex(pidx);
  }

  double Pivot::operator[](size_t i) const
  {
    assert(i<size());
    if (index().empty())
      return arg->atHCIndex(pivotIndex(i));
    return i<permutedIndex.size()? (*arg)[permutedIndex[i]]: nan("");
  }

  
  void PermuteAxis::setArgument(const TensorPtr& a,const Args& args)
  {
    arg=a;
    hypercube(arg->hypercube());
    m_index=arg->index();
    if (m_hypercube.xvectors.size()!=1) // ignore named axis for vectors
      for (m_axis=0; m_axis<m_hypercube.xvectors.size(); ++m_axis)
        if (m_hypercube.xvectors[m_axis].name==args.dimension)
          break;
    if (m_axis==m_hypercube.xvectors.size())
      throw runtime_error("axis "+args.dimension+" not found");
    for (size_t i=0; i<m_hypercube.xvectors[m_axis].size(); ++i)
      m_permutation.push_back(i);
  }

  void PermuteAxis::setPermutation(vector<size_t>&& p)
  {
    m_permutation=std::move(p);
    auto& xv=m_hypercube.xvectors[m_axis];
    xv.clear();
    auto& axv=arg->hypercube().xvectors[m_axis];
    for (auto i: m_permutation)
      if (i<axv.size())
        checkCancel(), xv.push_back(axv[i]);
    map<unsigned,unsigned> reverseIndex;
    for (size_t i=0; i<m_permutation.size(); checkCancel(), ++i)
      reverseIndex[m_permutation[i]]=i;
    map<size_t,size_t> indices;
    for (size_t i=0; i<arg->index().size(); checkCancel(), ++i)
      {
        auto splitted=arg->hypercube().splitIndex(arg->index()[i]);
        auto ri=reverseIndex.find(splitted[m_axis]);
        if (ri!=reverseIndex.end() && (splitted[m_axis]=ri->second)<axv.size())
          indices[hypercube().linealIndex(splitted)]=i;
      }
    m_index=indices;
    permutedIndex.clear();
    for (auto& i: indices) checkCancel(), permutedIndex.push_back(i.second);
  }
  
  double PermuteAxis::operator[](size_t i) const
  {
    assert(i<size());
    if (index().empty())
      {
        auto splitted=hypercube().splitIndex(i);
        if (m_axis>=splitted.size()) return nan("");
        splitted[m_axis]=m_permutation[splitted[m_axis]];
        return arg->atHCIndex(arg->hypercube().linealIndex(splitted));
      }
    return (*arg)[permutedIndex[i]];
  }

  void SpreadFirst::setSpreadDimensions(const Hypercube& hc)
  {
    if (!arg) return;
    if (hc.logNumElements()+hypercube().logNumElements()>64*log(2))
      throw std::runtime_error("Maximum hypercube exceeded");
    m_hypercube=hc;
    m_hypercube.xvectors.insert(m_hypercube.xvectors.end(), arg->hypercube().xvectors.begin(),
                                arg->hypercube().xvectors.end());
    numSpreadElements=hc.numElements();
    if (hc.rank()) m_index.clear();
  }

  void SpreadLast::setSpreadDimensions(const Hypercube& hc)
  {
    if (!arg) return;
    if (hc.logNumElements()+hypercube().logNumElements()>64*log(2))
      throw std::runtime_error("Maximum hypercube exceeded");
    m_hypercube=arg->hypercube();
    m_hypercube.xvectors.insert(m_hypercube.xvectors.end(), hc.xvectors.begin(),
                                hc.xvectors.end());
    numSpreadElements=arg->hypercube().numElements();
    if (hc.rank()) m_index.clear();
  }
  
  void SpreadFirst::setIndex()
  {
    if (!arg) return;
    auto& aIdx=arg->index();
    if (arg->index().empty()) return;
    if (numSpreadElements==1) {m_index=aIdx; return;}
    set<size_t> idx;
    for (auto i: aIdx)
      for (size_t j=0; j<numSpreadElements; checkCancel(), ++j)
        idx.insert(j+i*numSpreadElements);
    m_index=idx;
  }

  void SpreadLast::setIndex()
  {
    if (!arg) return;
    auto& aIdx=arg->index();
    if (arg->index().empty()) return;
    size_t numToSpread=1;
    set<size_t> idx;
    for (auto i=arg->rank(); i<rank(); ++i) numToSpread*=m_hypercube.xvectors[i].size();
    if (numToSpread==1) {m_index=aIdx; return;}
    for (size_t i=0; i<numToSpread; ++i)
      for (auto j: aIdx)
        checkCancel(), idx.insert(j+i*numSpreadElements);
    m_index=idx;
  }


  void SpreadOverHC::setArgument(const TensorPtr& a,const Args&) {
    if (a->rank()!=rank())
      throw std::runtime_error("mismatch of dimensions");
    for (size_t i=0; i<a->rank(); ++i)
      if (a->hypercube().xvectors[i].name!=hypercube().xvectors[i].name ||
          a->hypercube().xvectors[i].dimension.type!=hypercube().xvectors[i].dimension.type)
        throw std::runtime_error("mismatch of dimensions");

    arg=a;
    permutations.clear();
    permutations.resize(a->rank());
    for (size_t i=0; i<a->rank(); ++i)
      {
        map<any,size_t> srcIndices;
        for (size_t j=0; j<a->hypercube().xvectors[i].size(); checkCancel(), ++j)
          srcIndices[a->hypercube().xvectors[i][j]]=j;
        for (size_t j=0; j<hypercube().xvectors[i].size(); checkCancel(), ++j)
          {
            auto it=srcIndices.find(hypercube().xvectors[i][j]);
            if (it!=srcIndices.end())
              permutations[i].push_back(it->second);
            else
              permutations[i].push_back(numeric_limits<size_t>::max());
          }
      }
  }

    double SpreadOverHC::operator[](size_t idx) const {
      auto splitIdx=hypercube().splitIndex(index()[idx]);
      for (size_t i=0; i<splitIdx.size(); checkCancel(), ++i)
        {
          splitIdx[i]=permutations[i][splitIdx[i]];
          if (splitIdx[i]>=arg->hypercube().xvectors[i].size())
            return nan("");
        }
      return arg->atHCIndex(arg->hypercube().linealIndex(splitIdx));
    }

  namespace
  {
    ITensor::Timestamp maxTimestamp(const vector<TensorPtr>& x) {
      return x.size()?
        (*max_element(x.begin(), x.end(),
                      [](const TensorPtr& x,const TensorPtr& y){
                        return x->timestamp()<y->timestamp();
                      }))->timestamp():
        ITensor::Timestamp();
    }
  }
  
  Meld::Timestamp Meld::timestamp() const {return maxTimestamp(args);}

  void Meld::setArguments(const vector<TensorPtr>& a, const Args&)
  {
    if (a.empty()) return;
    args=a;
    hypercube(args[0]->hypercube());
    // all arguments must have the same hypercube
#ifndef NDEBUG
    for (auto& i: args)
      assert(i->hypercube()==hypercube());
#endif

    // create an index vector that is the union of the arguments' index vectors
    if (all_of(args.begin(), args.end(), [](const TensorPtr& i) {return !i->index().empty();}))
      {
        std::set<std::size_t> tmp_index;
        for (auto& i: args)
          {
            for (auto j: i->index())
              checkCancel(), tmp_index.insert(j);
          }
        m_index=tmp_index;
      }
  }

  double Meld::operator[](size_t idx) const {
    size_t hcIndex=m_index[idx];
    for (auto& i: args)
      {
        auto val=i->atHCIndex(hcIndex);
        if (isfinite(val))
          return val;
      }
    return nan("");
  }

  Merge::Timestamp Merge::timestamp() const {return maxTimestamp(args);}

  void Merge::setArguments(const vector<TensorPtr>& a, const Args& opArgs)
  {
    if (a.empty()) return;
    args=a;
    // all arguments must have the same hypercube
#ifndef NDEBUG
    for (auto& i: args)
      assert(i->hypercube()==args.front()->hypercube());
#endif
    // extend into the next dimension
    auto hc=args.front()->hypercube();
    hc.xvectors.emplace_back(opArgs.dimension);
    for (size_t i=0; i<args.size(); ++i)
      hc.xvectors.back().push_back(to_string(i));
    hypercube(std::move(hc));

    if (hypercube().logNumElements()<log(numeric_limits<size_t>::max())) // make sure we can fit in a size_t
      {
        size_t total=0;
        for (auto& i: args) total+=i->size();
        size_t sliceSize=args.front()->hypercube().numElements();
        if (total<hypercube().numElements()/2)
          {
            // create an index vector that combines the arguments' index vectors
            std::set<std::size_t> tmp_index;
            for (size_t i=0; i<args.size(); ++i)
              {
                if (args[i]->index().empty())
                  for (size_t j=0; j<args[i]->size(); checkCancel(), ++j)
                    tmp_index.insert(i*sliceSize+j);
                else
                  for (auto j: args[i]->index())
                    checkCancel(), tmp_index.insert(i*sliceSize+j);
              }
            m_index=std::move(tmp_index);
          }
      }
  }

  double Merge::operator[](size_t i) const {
    if (args.empty()) return nan("");
    auto res=lldiv(m_index[i], args[0]->hypercube().numElements());
    return args[res.quot]->atHCIndex(res.rem);
  }
}
