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

#ifndef CIVITA_TENSORINTERFACE_H
#define CIVITA_TENSORINTERFACE_H
#include "hypercube.h"
#include "index.h"

#ifndef CLASSDESC_ACCESS
#define CLASSDESC_ACCESS(x)
#endif
#include <atomic>
#include <chrono>
#include <iostream>
#include <set>

namespace civita
{
  class ITensor;
  using TensorPtr=std::shared_ptr<ITensor>;

  class ITensor
  {
  public:
    CLASSDESC_ACCESS(ITensor);
    ITensor() {}
    ITensor(const Hypercube& hc): m_hypercube(hc) {}
    ITensor(Hypercube&& hc): m_hypercube(std::move(hc)) {}
    ITensor(const std::vector<unsigned>& dims) {m_hypercube.dims(dims);}
    ITensor(const ITensor&)=default;
    ITensor(ITensor&&)=default;
    ITensor& operator=(const ITensor&)=default;
    ITensor& operator=(ITensor&&)=default;
    virtual ~ITensor() {}
    /// information describing the axes, types and labels of this tensor
    virtual const Hypercube& hypercube() const {return m_hypercube;}
    virtual const Hypercube& hypercube(const Hypercube& hc) {return m_hypercube=hc;}
    virtual const Hypercube& hypercube(Hypercube&& hc) {return m_hypercube=std::move(hc);}
    std::size_t rank() const {return hypercube().rank();}
    std::vector<unsigned> shape() const {return hypercube().dims();}

    /// impose dimensions according to dimension map \a dimensions
    void imposeDimensions(const Dimensions& dimensions) {
      auto hc=hypercube();
      for (auto& xv: hc.xvectors)
        {
          auto dim=dimensions.find(xv.name);
          if (dim!=dimensions.end())
            {
              xv.dimension=dim->second;
              xv.imposeDimension();
            }
        }
      hypercube(std::move(hc));
    }
    
    /// the index vector - assumed to be ordered and unique
    virtual const Index& index() const {return m_index;}
    /// return or compute data at a location
    virtual double operator[](std::size_t) const=0;
    /// word version to allow access from scripts
    double at(std::size_t i) const {return (*this)[i];}
    /// return vector of data ([0]..[size()-1])
    std::vector<double> data() const {
      std::vector<double> r; r.reserve(size());
      for (size_t i=0; i<size(); ++i)
        r.push_back(at(i));
      return r;
    }
    /// return number of elements in tensor - maybe less than hypercube.numElements if sparse
    virtual std::size_t size() const {
      std::size_t s=index().size();
      return s? s: hypercube().numElements();
    }        
    
    /// returns the data value at hypercube index \a hcIdx, or NaN if 
    double atHCIndex(std::size_t hcIdx) const {
      auto& idx=index();
      if (idx.empty()) {// this is dense
        if (hcIdx<size())
          return (*this)[hcIdx]; 
      } else {
        auto i=idx.linealOffset(hcIdx);
        if (i<idx.size())
          return (*this)[i];
      }
      return nan(""); //element not found
    }

    template <class T>
    std::size_t hcIndex(const std::initializer_list<T>& indices) const
    {return hypercube().linealIndex(indices);}

    template <class T>
    double operator()(const std::initializer_list<T>& indices) const
    {return atHCIndex(hcIndex(indices));}

    using Timestamp=std::chrono::time_point<std::chrono::high_resolution_clock>;
    /// timestamp indicating how old the dependendent data might
    /// be. Used in CachedTensorOp to determine when to invalidate the
    /// cache
    virtual civita::ITensor::Timestamp timestamp() const=0;

    /// arguments relevant for tensor expressions, not always meaningful. Exception thrown if not.
    struct Args
    {
      std::string dimension;
      double val;
    };
    virtual void setArgument(const TensorPtr&, const ITensor::Args& args={"",0})  {notImpl();}
    virtual void setArguments(const TensorPtr&, const TensorPtr&,
                              const ITensor::Args& args={}) {notImpl();}
    virtual void setArguments(const std::vector<TensorPtr>& a,
                              const ITensor::Args& args={"",0}) 
    {if (a.size()) setArgument(a[0], args);}
    virtual void setArguments(const std::vector<TensorPtr>& a1,
                              const std::vector<TensorPtr>& a2,
                              const ITensor::Args& args={"",0})
    {setArguments(a1.empty()? TensorPtr(): a1[0], a2.empty()? TensorPtr(): a2[0], args);}

    struct Cancelled: public std::exception {
      const char* what() const noexcept override {return "civita cancelled";}
    };
  
    /// Can be used to terminate long running computations from another thread.
    /// @param v true to cancel the computation, false to reset the cancel condition
    /// @throw Cancelled will be thrown from the cancelled thread
    /// Note - setting this to true will cancel all civita computations in all threads
    static void cancel(bool v) {s_cancel=v;}

    /// checks if operation has been cancelled, and throws and exception if so
    void checkCancel() const {if (s_cancel.load()) throw Cancelled();}
    
  protected:
    Hypercube m_hypercube;
    Index m_index;
    static std::atomic<bool> s_cancel;
    void notImpl() const
    {throw std::runtime_error("setArgument(s) variant not implemented");}
  };

  /// wraps an ITensor referance - useful for creating a TensorPtr referring to a reference
  class ITensorRef: public ITensor
  {
    ITensor& ref;
  public:
    ITensorRef(ITensor& ref): ref(ref) {}
    const Hypercube& hypercube() const override {return ref.hypercube();}
    const Hypercube& hypercube(const Hypercube& hc) override {return ref.hypercube(hc);}
    const Hypercube& hypercube(Hypercube&& hc) override {return ref.hypercube(std::move(hc));}
    const Index& index() const override {return ref.index();}
    double operator[](std::size_t i) const override {return ref[i];}
    std::size_t size() const override {return ref.size();}
    civita::ITensor::Timestamp timestamp() const override {return ref.timestamp();}
  };
     
  inline std::ostream& operator<<(std::ostream& o, const ITensor::Timestamp& t)
  {
     return o<<std::chrono::system_clock::to_time_t
      // use now() to convert between clock types
      (std::chrono::system_clock::now()+
       std::chrono::duration_cast<std::chrono::system_clock::duration>
       ((t-ITensor::Timestamp::clock::now())));
   }

  inline void printAtHCIndex(const ITensor& t, std::ostream& o, std::size_t hcIdx)
    {
      auto& hc=t.hypercube();
      auto splitIndex=hc.splitIndex(hcIdx);
      o<<"[";
      for (size_t i=0; i<splitIndex.size(); ++i)
        o<<civita::str(hc.xvectors[i][splitIndex[i]], hc.xvectors[i].dimension.units)<<" ";
      o<<"]="<<t.atHCIndex(hcIdx);
    }

  inline void printAtIndex(const ITensor& t, std::ostream& o, std::size_t idx)
    {
      if (t.index().empty())
        printAtHCIndex(t,o,idx);
      else
        printAtHCIndex(t,o,t.index()[idx]);
    }
    
}

#endif
