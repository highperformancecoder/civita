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

#ifndef CIVITA_TENSORVAL_H
#define CIVITA_TENSORVAL_H

#include "tensorInterface.h"
#include <vector>
#include <chrono>

#ifndef CIVITA_ALLOCATOR
#define CIVITA_ALLOCATOR std::allocator
#endif

namespace civita
{
  /// abstraction of a tensor variable, stored in contiguous memory
  struct ITensorVal: public ITensor
  {
    ITensorVal() {}
    ITensorVal(const Hypercube& hc): ITensor(hc) {}
    ITensorVal(Hypercube&& hc): ITensor(std::move(hc)) {}
    ITensorVal(const std::vector<unsigned>& dims): ITensor(dims) {}
    virtual const ITensorVal& operator=(const ITensor&)=0;
    template <class T>
    ITensorVal& operator=(const std::initializer_list<T>& vals) {
      auto i=begin();
      for (auto j: vals)
        *i++=j;
      return *this;
    }

    virtual double& operator[](std::size_t)=0;
    using ITensor::operator[];
    using ITensor::operator();
    template <class T>
    double& operator()(const std::initializer_list<T>& indices)
    {
      auto idx=index();
      auto hcIdx=hcIndex(indices);
      if (idx.empty())
        return operator[](hcIdx);
      else
        {
          auto i=idx.linealOffset(hcIdx);
          if (i<size()) return operator[](i); 
          static double noValue=nan("");
          return noValue;
        }
    }
          
    const Index& index(const std::initializer_list<std::size_t>& x) {
      std::set<std::size_t,std::less<std::size_t>,CIVITA_ALLOCATOR<std::size_t>>
        tmp(x.begin(), x.end());
      return index(Index(tmp));
    }
    const Index& index(const Index& x) {auto tmp=x; return index(std::move(tmp));}
    template <class T>
    const Index& index(const T& x) {return index(Index(x));}
    virtual const Index& index(Index&&)=0;
    using ITensor::index;
    
    typedef double* iterator;
    typedef const double* const_iterator;

    const_iterator begin() const {return const_cast<ITensorVal*>(this)->begin();}
    const_iterator end() const {return begin()+size();}
    iterator begin() {return size()? &((*this)[0]): nullptr;}
    iterator end() {return begin()+size();}
  };
  
  /// represent a tensor in initialisation expressions
  class TensorVal: public ITensorVal
  {
    std::vector<double,CIVITA_ALLOCATOR<double>> data;
    Timestamp m_timestamp;
    CLASSDESC_ACCESS(TensorVal);
    void assignDenseOrSparse(const std::map<std::size_t,double>& x) {
      size_t ne=m_hypercube.numElements();
      if (2*x.size()<ne)
        *this=x;
      else {
        m_index.clear(); data.clear(); data.resize(ne,std::nan(""));
        for (auto& i: x) (*this)[i.first]=i.second;
      }
    }
  public:
    TensorVal(): data(1) {}
    TensorVal(double x): data(1,x) {}
    TensorVal(const Hypercube& hc): ITensorVal(hc) {allocVal();}
    TensorVal(Hypercube&& hc): ITensorVal(std::move(hc)) {allocVal();}
    TensorVal(const std::vector<unsigned>& dims): ITensorVal(dims) {allocVal();}
    TensorVal(const ITensor& t) {*this=t;}
    
    using ITensorVal::index;
    const Index& index(Index&& idx) override {
      m_index=std::move(idx);
      allocVal();
      return m_index;
    }
    const Hypercube& hypercube(const Hypercube& hc) override
    {m_hypercube=hc; allocVal(); return m_hypercube;}
    const Hypercube& hypercube(Hypercube&& hc) override 
    {m_hypercube=std::move(hc);allocVal();return m_hypercube;}
    using ITensor::hypercube;

    // for javascript support
    void setDimensions(const std::vector<unsigned>& dims) {
      m_hypercube.dims(dims); allocVal();
    }
    void setHypercube(const Hypercube& hc) {hypercube(hc);}
    
    void allocVal() {data.resize(size());}

    // assign a sparse data set
    void assign(const std::map<std::size_t,double>& x) {*this=x;}
    void assign(const Hypercube& hc, const std::map<std::size_t,double>& x)
    {m_hypercube=hc; assignDenseOrSparse(x);}
    void assign(Hypercube&& hc, const std::map<std::size_t,double>& x)
    {m_hypercube=std::move(hc); assignDenseOrSparse(x);}

    template <class A>
    TensorVal& operator=(const std::map<std::size_t,double,std::less<std::size_t>,A>& x) {
      m_index=x;
      data.clear(); data.reserve(x.size());
      for (auto& j: x) data.push_back(j.second);
      updateTimestamp();
      return *this;
    }

    // assign a dense data set. Note data is trimmed or padded to hypercube().numElements();
    void assign(const std::vector<double>& x) {*this=x;}
    
    template <class A>
    TensorVal& operator=(const std::vector<double,A>& x) {
      data.resize(x.size());
      std::memcpy(data.data(),x.data(),sizeof(double)*x.size());
      allocVal(); updateTimestamp();
      return *this;
    }
    
    double operator[](std::size_t i) const override {return data.empty()? 0: data[i];}
    double& operator[](std::size_t i) override {updateTimestamp(); return data[i];}
    const TensorVal& operator=(const ITensor& x) override {
      index(x.index());
      hypercube(x.hypercube());
      assert(data.size()==x.size());
      for (std::size_t i=0; i<x.size(); ++i) data[i]=x[i];
      updateTimestamp();
      return *this;
    }

    ITensor::Timestamp timestamp() const override {return m_timestamp;}
    // timestamp should be updated every time the data r index vectors
    // is updated, if using the CachedTensorOp functionality
    void updateTimestamp() {m_timestamp=std::chrono::high_resolution_clock::now();}
  };

  /// for use in Minsky init expressions
  inline TensorVal operator*(double a, const TensorVal& x)
  {
    TensorVal r(x);
    for (auto& i: r) i*=a;
    return r;
  }

  /// output a summary of dimensions, types and units of the hypercube
  inline std::ostream& operator<<(std::ostream& o, const TensorVal& x)
  {
    static const char* dimNames[]={"string","time","value"};
    o<<"[";
    for (auto& i: x.hypercube().xvectors)
      o<<"{"<<i.name<<"("<<i.size()<<"):"<<dimNames[i.dimension.type]<<" "<<i.dimension.units<<"},";
    return o<<"]";
  }
}

#endif
